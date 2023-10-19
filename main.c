/*
  Created by:
    Samuel Pires - 2019225195
    Sofia Neves  - 2019220082
*/


#include "projeto.h"

int main()
{
  //pidmain = getpid();
  sigfillset(&block_set); // will have all possible signals blocked when our handler is called

  //define a handler for SIGINT, SIGTSTP and SIGUSR1; when entered all possible signals are blocked
  new_action.sa_flags = 0;
  new_action.sa_mask = block_set;
  new_action.sa_handler = SIG_IGN;
  sigaction(SIGINT,&new_action,NULL);
  sigaction(SIGTSTP,&new_action,NULL);
  sigaction(SIGUSR1,&new_action,NULL);


  //signal(SIGTSTP, statistic);
  pid_t pid_race, pid_malfunction;
  char log_file_name[8] = "log.txt";

  printf("PID MAIN: %d\n", getpid());

  // Opening log file
  if( (log_file = fopen(log_file_name, "w")) == NULL){
    printf("ERROR WITH FOPEN %s FILE\n", log_file_name);
  }


  // Creating shared memory and extracting data from config file
  initialize();

  new_action.sa_handler = &termination_handler;
  sigaction(SIGINT,&new_action,NULL);
  sigaction(SIGTSTP,&new_action,NULL);

  // Race Manager process
  if((pid_race = fork()) == 0)
  {
    race_manager();
    exit(0);
  }else if(pid_race == -1)
  {
    cleanup();
  }

  // Malfunction manager process
  if((pid_malfunction = fork()) == 0)
  {
    malfunction_manager();
    exit(0);
  }else if(pid_malfunction == -1){
    cleanup();
  }

  for(int i = 0; i < 2; i++) wait(NULL); // parent waits for all children
  
  // Closing semaphores, shared memory, processes and files
  cleanup();
  return 0;
}


/* -------------------------- WRITE TO LOG AND CONSOLE ------------------- */
void write_to_log_and_console(char *text)
{
  sem_wait(print);

  now = time(NULL);
  tm_struct = localtime(&now);  

  fprintf(log_file, "%2d:%2d:%2d %s\n", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec, text);
  fflush(log_file);
  fflush(stdout);
  printf("%2d:%2d:%2d %s\n", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec, text);
  fflush(stdout);

  sem_post(print);
}


/* ------------------------ EXTRACT DATA FROM CONFIG FILE -------------- */
boolean check_config_file_data(FILE *config_file)
{
  int list_size = 3, line_num = 0, list[list_size];
  char line[LINE_SIZE], string[LINE_SIZE];
  char* aux = "ERROR WITH CONFIG FILE LINE %d\n";
  while(fgets(line, LINE_SIZE, config_file) != NULL) {
    
    line_num += 1;
    sprintf(string, aux, line_num);
    switch (line_num) {
      case 1:
              separate_line(line, list_size, list);
              if(list[0] <= 0) {
                write_to_log_and_console(string);
                write_to_log_and_console("SIMULATOR CLOSING\n");
                return false;
              }
              else{ 
                config.timeUnits = list[0]; 
              }
              break;  
      case 2:
              separate_line(line, list_size, list);
              if(list[0] <= 0 || list[1] <= 0) {
                write_to_log_and_console(string);
                write_to_log_and_console("SIMULATOR CLOSING\n");
                return false;
              }
              else { 
                config.distance = list[0]; 
                config.laps = list[1];
              }
              break;
      case 3:
              separate_line(line, list_size, list);
              if(list[0] <= 0) {
                write_to_log_and_console(string);
                write_to_log_and_console("SIMULATOR CLOSING\n");
                return false;
              }
              else{ config.numTeams = list[0]; } 
              break;
      case 4:
              separate_line(line, list_size, list);
              if(list[0] <= 0) {
                write_to_log_and_console(string);
                write_to_log_and_console("SIMULATOR CLOSING\n");

                return false;
              }
              else{  config.numCars = list[0]; } 
              break;
      case 5:
              separate_line(line, list_size, list);
              if(list[0] <= 0) {
                write_to_log_and_console(string);
                write_to_log_and_console("SIMULATOR CLOSING\n");

                return false;
              }
              else{ config.T_Avaria = list[0]; }
              break;
      case 6:
              separate_line(line, list_size, list);
              if(list[0] <= 0 || list[1] <= 0) {
                write_to_log_and_console(string);
                write_to_log_and_console("SIMULATOR CLOSING\n");
                return false;
              }
              else {
                 config.T_Box_Min = list[0];
                 config.T_Box_Max = list[1];
              }
              break;
      case 7: 
              separate_line(line, list_size, list);
              if(list[0] <= 0) {
                write_to_log_and_console(string);
                write_to_log_and_console("SIMULATOR CLOSING\n");

                return false;
              }
              else{ config.deposit_capacity = list[0]; }
              break;
    }
  }
  return true;
}


/* ------------------------- INITIALIZE AND CLEANUP FUNCTIONS ----------- */
void initialize()
{
  char config_file_name[11] = "config.txt", string[LINE_SIZE];

  // Named Semaphores
  sem_unlink("PRINT");
  print = sem_open("PRINT", O_CREAT|O_EXCL, 0766, 1);
  sem_unlink("GO");
  go = sem_open("GO", O_CREAT|O_EXCL, 0766, 0);

  write_to_log_and_console("SIMULATOR STARTING\n");

  sem_unlink("STOP_WRITERS");
  stop_writers = sem_open("STOP_WRITERS", O_CREAT|O_EXCL, 0766, 1);

  sem_unlink("READ_SHARED_VAR");
  read_shared_var = sem_open("READ_SHARED_VAR", O_CREAT|O_EXCL, 0766, 1);

  // Opens configuration file and Initializes values of struct Config
  FILE *config_file;
  if( (config_file = fopen(config_file_name, "r")) == NULL){
    sprintf(string, "ERROR WITH FOPEN %s FILE\n", config_file_name);
    write_to_log_and_console(string);
    exit(EXIT_FAILURE);
  }
  
  if( !check_config_file_data(config_file) )
  {
    exit(EXIT_FAILURE);
  }
  fclose(config_file);


  // Allocates memory for struct shared_Memory
	if ((shmid = shmget(IPC_PRIVATE,
                      sizeof(struct SharedMemory) +
                      sizeof(struct Statistics) + 
                      sizeof(struct Team) * config.numTeams +
                      sizeof(struct Car) * config.numTeams * config.numCars, 
                      IPC_CREAT|0777)) == -1)
  {
    write_to_log_and_console("ERROR WITH SHMGET FOR STRUCT shared_Memory\n");
    write_to_log_and_console("SIMULATOR CLOSING\n");
    exit(EXIT_FAILURE);
  }


  // Allocates memory for struct Config
  if ((shared_var = (struct SharedMemory *) shmat(shmid, NULL, 0)) == (struct SharedMemory *)-1)
  {
    write_to_log_and_console("ERROR WITH SHMAT FOR STRUCT Statistics\n");
    write_to_log_and_console("SIMULATOR CLOSING\n");
    exit(EXIT_FAILURE);
  }
  statistic = (struct Statistics *) (shared_var + 1);

  // Allocates memory for struct Team
  teams = (struct Team *) (statistic + 1);

  // Allocates memory for all struct Car 
  cars = (struct Car *) (teams + config.numTeams);
  

    // Conditional Variables
    /* Initialize attribute of mutex. */
  pthread_mutexattr_init(&mutex_signal_start_race);
  pthread_mutexattr_setpshared(&mutex_signal_start_race, PTHREAD_PROCESS_SHARED);


  pthread_mutexattr_init(&mutex_signal_finish_race);
  pthread_mutexattr_setpshared(&mutex_signal_finish_race, PTHREAD_PROCESS_SHARED);


  pthread_mutexattr_init(&mutex_signal_malfunction_man);
  pthread_mutexattr_setpshared(&mutex_signal_malfunction_man, PTHREAD_PROCESS_SHARED);
  
  
    /* Initialize attribute of condition variable. */
  pthread_condattr_init(&attrcondv_signal_start_race);
  pthread_condattr_setpshared(&attrcondv_signal_start_race, PTHREAD_PROCESS_SHARED);
    /* Initialize attribute of condition variable. */
  pthread_condattr_init(&attrcondv_signal_finish_race);
  pthread_condattr_setpshared(&attrcondv_signal_finish_race, PTHREAD_PROCESS_SHARED);
    /* Initialize attribute of condition variable. */
  pthread_condattr_init(&attrcondv_signal_malfunction_man);
  pthread_condattr_setpshared(&attrcondv_signal_malfunction_man, PTHREAD_PROCESS_SHARED);

  

  /* Initialize mutex. */
  pthread_mutex_init(&shared_var->mutex_start_race, &mutex_signal_start_race);
  pthread_mutex_init(&shared_var->mutex_finish_race, &mutex_signal_finish_race);
  pthread_mutex_init(&shared_var->mutex_malfunction_man, &mutex_signal_malfunction_man);
  /* Initialize condition variables. */
  pthread_cond_init(&shared_var->start_race, &attrcondv_signal_start_race);
  pthread_cond_init(&shared_var->finish_race, &attrcondv_signal_finish_race);
  pthread_cond_init(&shared_var->malfunction_man, &attrcondv_signal_malfunction_man);

  // Unnamed Semaphores
  for(int i = 0; i < config.numTeams * config.numCars; ++i){
    sem_init(&cars[i].mutex_start_car, 1, 0);
    sem_init(&cars[i].mutex_finish_car, 1, 0);
  }

  for(int i = 0; i < config.numTeams; ++i){
    sem_init(&teams[i].mutex_start_team, 1, 0);
    sem_init(&teams[i].mutex_finish_team, 1, 0);
  }


  // Create message queue
  mqid = msgget(IPC_PRIVATE, IPC_CREAT|0777);
 

  //Start cars at 0
  shared_var->car_numbers = (int *) malloc(sizeof(int)*config.numTeams*config.numCars);
  for(int i = 0; i < config.numTeams * config.numCars; ++i){
    cars[i].car_num = 0;
    shared_var->car_numbers[i] = 0;
  }
  

  //Start shared_var
  shared_var->has_race_started = false;
  shared_var->cars_numbers_index = 0;
  shared_var->n_readers = 0;
  shared_var->teams_in_race = 0;
  shared_var->is_EINTR_set = 0; 
  statistic->position = 0;
  

  
  //create nammed pipe
	if(mkfifo(NAMED_PIPE_NAME, O_CREAT|O_EXCL|0777) < 0){
		write_to_log_and_console("CANNOT CREATE NAMED PIPE\n");
    write_to_log_and_console("SIMULATOR CLOSING\n");
    exit(EXIT_FAILURE);
	}
  
  if((named_pipe = open(NAMED_PIPE_NAME, O_RDWR)) < 0){
		write_to_log_and_console("CANNOT OPEN NAMED PIPE\n");
    write_to_log_and_console("SIMULATOR CLOSING\n");
    exit(EXIT_FAILURE);
	}

  // Open unnamed pipe for every car
  for(int i = 0; i < config.numTeams * config.numCars; ++i)
  {
    pipe(cars[i].fd);
  }
}




void cleanup()
{
  // Eliminates Conditional Variables
  pthread_mutex_destroy(&shared_var->mutex_start_race);
  pthread_mutex_destroy(&shared_var->mutex_finish_race);
  pthread_mutex_destroy(&shared_var->mutex_malfunction_man);


  pthread_mutexattr_destroy(&mutex_signal_start_race);
  pthread_mutexattr_destroy(&mutex_signal_finish_race);
  pthread_mutexattr_destroy(&mutex_signal_malfunction_man);


  pthread_cond_destroy(&shared_var->start_race);
  pthread_cond_destroy(&shared_var->finish_race);
  pthread_cond_destroy(&shared_var->malfunction_man);


  pthread_condattr_destroy(&attrcondv_signal_start_race);
  pthread_condattr_destroy(&attrcondv_signal_finish_race);
  pthread_condattr_destroy(&attrcondv_signal_malfunction_man);


  //Eliminates unnamed semaphores
  for(int i = 0; i < config.numTeams * config.numCars; ++i){
    sem_destroy(&cars[i].mutex_start_car);
    sem_destroy(&cars[i].mutex_finish_car);

    close(cars[i].fd[0]);
    close(cars[i].fd[1]);
  }

  for(int i = 0; i < config.numTeams; ++i){
    sem_destroy(&teams[i].mutex_start_team);
    sem_destroy(&teams[i].mutex_finish_team);
  }

  // Eliminates message queue
  msgctl(mqid, IPC_RMID, 0);

  // Clear nammed pipe
  close(named_pipe);
  unlink(NAMED_PIPE_NAME);

 
  // Eliminates shared memory
  if(shmid >= 0)
  {
    shmdt(shared_var);
    shmctl(shmid, IPC_RMID, NULL);
  }

  // Eliminates named semaphores
  
  sem_close(go);
  sem_unlink("GO");
  sem_close(stop_writers);
  sem_unlink("STOP_WRITERS");
  sem_close(read_shared_var);
  sem_unlink("READ_SHARED_VAR");

  write_to_log_and_console("SIMULATOR CLOSING\n");
  fclose(log_file);
  sem_close(print);
  sem_unlink("PRINT");

  exit(0);
}



/* ------------------------- READ AND WRITE FROM SHARED MEMORY ---------- */
void write_float_SharedMemory(float *address, float value) {
  sem_wait(stop_writers);
  *address = value;
  sem_post(stop_writers);
}

void write_int_SharedMemory(int *address, int value) {
  sem_wait(stop_writers);
  *address = value;
  sem_post(stop_writers);
}

int read_int_SharedMemory(int *address) {
  int value;

  sem_wait(read_shared_var);
  ++shared_var->n_readers;
  if (shared_var->n_readers == 1)
    sem_wait(stop_writers);
  sem_post(read_shared_var);

  value = *address;

  sem_wait(read_shared_var);
  --shared_var->n_readers;
  if (shared_var->n_readers == 0)
  sem_post(stop_writers);
  sem_post(read_shared_var);
  return value;
}

float read_float_SharedMemory(float *address) {
  float value;

  sem_wait(read_shared_var);
  ++shared_var->n_readers;
  if (shared_var->n_readers == 1)
    sem_wait(stop_writers);
  sem_post(read_shared_var);

  value = *address;

  sem_wait(read_shared_var);
  --shared_var->n_readers;
  if (shared_var->n_readers == 0)
  sem_post(stop_writers);
  sem_post(read_shared_var);
  return value;
}




/* --------------------------- AUXILIARY FUNCTIONS ----------------------- */
void separate_line(char *line, int list_size, int list[list_size]){
  int i = 0;
  char *token = strtok(line, ", ");
  while(token != NULL) {
    list[i++] = check_if_digit(token);
    token = strtok(NULL, ", ");
  }
}

int check_if_digit(char *word){
  int number = 0;
  for(int i = 0; word[i] != '\0'; ++i){
    if(isdigit(word[i])){
      number = number*10 + (word[i] - '0');
    } 
    else{
     return number;
    }
  }
  return number;
}


