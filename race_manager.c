/*
  Created by:
    Samuel Pires - 2019225195
    Sofia Neves  - 2019220082
*/


#include "projeto.h"


void race_manager()
{
  new_action.sa_handler = SIG_IGN;
  sigaction(SIGINT,&new_action,NULL);
  sigaction(SIGTSTP,&new_action,NULL);


  pid_t pid;
  pthread_t thread_named_pipe;
  int num_cars_in_race,
      num_teams_in_race,
      count_avaria = 0;

  

  pthread_create(&thread_named_pipe, NULL, pipe_manager, NULL);

  
  pthread_mutex_lock(&shared_var->mutex_start_race);
  while(read_int_SharedMemory(&shared_var->has_race_started) != 1){
    pthread_cond_wait(&shared_var->start_race, &shared_var->mutex_start_race);
  }
  pthread_mutex_unlock(&shared_var->mutex_start_race);
  if(read_int_SharedMemory(&shared_var->is_EINTR_set) == 1){
    char end[LINE_SIZE] = "QUIT NAMED PIPE";
    write(named_pipe, &end, sizeof(end));
    pthread_join(thread_named_pipe, NULL);
    exit(0);
  }

  // Create processes team manager for each team
  for (int i = 0; i < config.numTeams; ++i)
  {
    pid = fork();
    if (pid == 0)
    {
      team_manager(&teams[i], i);
      exit(0);
    }else if(pid == -1){
      // cleanup(); <--------------- PENSAR
    }
  }

  int *indexes_cars = (int *) malloc(sizeof(int)*config.numCars*config.numTeams);
  int *indexes_teams = (int *) malloc(sizeof(int)*config.numTeams);

  num_cars_in_race = read_int_SharedMemory(&statistic->cars_in_race);
  get_indexes_of_cars_in_race(indexes_cars);
  for(int i = 0; i < num_cars_in_race; ++i){
    sem_wait(&cars[indexes_cars[i]].mutex_finish_car);
  }
  clean_indexes_cars(indexes_cars);

  int count = 1;
  while(true){
    count_avaria = count_avaria + config.timeUnits;
    if(count_avaria >= config.T_Avaria){
      pthread_cond_signal(&shared_var->malfunction_man);
      count_avaria = 0;
    }
    else{
      sem_post(go);
    }

    // Cars
    sem_wait(go);
    num_cars_in_race = read_int_SharedMemory(&statistic->cars_in_race);
    get_indexes_of_cars_in_race(indexes_cars);
    for(int i = 0; i < num_cars_in_race; ++i){
      
      sem_post(&cars[indexes_cars[i]].mutex_start_car);
    }

    for(int i = 0; i < num_cars_in_race; ++i){
      sem_wait(&cars[indexes_cars[i]].mutex_finish_car);
    }
    clean_indexes_cars(indexes_cars);


    //Team Manager
    num_teams_in_race = get_indexes_of_teams_in_race(indexes_teams);
    write_int_SharedMemory(&shared_var->teams_in_race, num_teams_in_race);

    if(num_teams_in_race == 0){
      for(int i = 0; i < config.numTeams; ++i){
        sem_post(&teams[i].mutex_start_team);
      }
      pthread_cond_signal(&shared_var->malfunction_man);
      char end[LINE_SIZE] = "QUIT NAMED PIPE";
      write(named_pipe, &end, sizeof(end));
      break;
    }

    for(int i = 0; i < num_teams_in_race; ++i){
      sem_post(&teams[indexes_teams[i]].mutex_start_team);
    }

    for(int i = 0; i < num_teams_in_race; ++i){
      sem_wait(&teams[indexes_teams[i]].mutex_finish_team);
    }
    clean_indexes_teams(indexes_teams);

    
    //print_cars();

    count++;
  }
  
  // --------------------- ACABOU ------------------------
  print_cars();

  pthread_join(thread_named_pipe, NULL);
  // wait for all processes to finish
  for (int i = 0; i < config.numTeams; i++){
    wait(NULL);
  }

  pthread_cond_signal(&shared_var->start_race);

}



/* -------------------------- THREAD THAT DEALS WITH PIPES ------------------- */
void *pipe_manager(void *id)
{
  fd_set read_set;
  int number_of_chars;
  char str_named_pipe[LINE_SIZE], str_unnamed_pipe[LINE_SIZE];


  // Read from pipes
  while (true)
  {
    // I/O Multiplexing
    FD_ZERO(&read_set);
    for(int i = 0; i < config.numTeams * config.numCars; ++i)
      FD_SET(cars[i].fd[0], &read_set);
    FD_SET(named_pipe, &read_set);
    
    if(select(named_pipe + 1, &read_set, NULL, NULL, NULL) > 0){
      if(FD_ISSET(named_pipe, &read_set)) 
      {
        if(read_int_SharedMemory(&shared_var->has_race_started) == 1
            && read_int_SharedMemory(&statistic->cars_in_race) == 0)
        {
          break;
        }
        
        number_of_chars = read(named_pipe, str_named_pipe, sizeof(str_named_pipe));
        str_named_pipe[number_of_chars-1] = '\0'; 

        // Deal with line
        check_named_pipe_data(str_named_pipe);
      }

      for(int i = 0; i < config.numTeams * config.numCars; ++i)
      {
        if (FD_ISSET(cars[i].fd[0], &read_set)){
          number_of_chars = read(cars[i].fd[0], &str_unnamed_pipe, sizeof(str_unnamed_pipe));
          str_unnamed_pipe[number_of_chars-1] = '\0';
          write_to_log_and_console(str_unnamed_pipe);
        }
      }
    }
  }
  write_to_log_and_console(str_named_pipe);
  pthread_exit(NULL);
}



void check_named_pipe_data(char line[LINE_SIZE])
{
  // Check line by line in file
  int flag_wrong_line = 0;
  int rows = 5, columns = 50;
  char car[rows][columns], 
       string[LINE_SIZE], 
       aux[LINE_SIZE-50];
  char *wrong_string = "WRONG COMMAND => %s\n";

  if(strcmp(line, "\0") != 0) { 
    line[strcspn(line, "\r")] = 0;

    sprintf(string, wrong_string, line);
    strcpy(aux, line);

    
    if( read_int_SharedMemory(&shared_var->has_race_started) == 1 ){
      write_to_log_and_console("REJECTED, RACE ALREADY STARTED\n");
      return;
    }

    if( (strcmp(line, "START RACE!")) == 0)
    {
      write_to_log_and_console("NEW COMMAND RECEIVED: START RACE!\n");
      if(read_int_SharedMemory(&shared_var->teams_in_race) < config.numTeams)
      {
        write_to_log_and_console("CANNOT START, NOT ENOUGH TEAMS\n");
      }
      else
      {
        write_int_SharedMemory(&shared_var->has_race_started, 1);
        prepare_cars_for_race();
        pthread_cond_broadcast(&shared_var->start_race);
      }
      return;
    }


    int position = 0, car_index = 0;  

    // Separates file by ', '
    char *token1 = strtok(line, " ");
    while(token1 != NULL)
    {
      // TEAM
      if(position == 0 && strcmp("ADDCAR", token1) != 0)
      {
        write_to_log_and_console(string);
        flag_wrong_line = 1;
        break;
      }
      else if(position == 1 && strcmp("TEAM:", token1) != 0)
      {
        write_to_log_and_console(string);
        flag_wrong_line = 1;
        break;
      }
      else if(position == 2)
      {
        leave_alphanumeric_chars(token1);
        strcpy(car[car_index++], token1);
      }
      // CAR
      else if(position == 3 && strcmp("CAR:", token1) != 0)
      {
        write_to_log_and_console(string);
        flag_wrong_line = 1;
        break;
      }
      else if(position == 4)
      {
        leave_numeric_chars(token1);
        strcpy(car[car_index++], token1);
      }
      // SPEED
      else if(position == 5 && strcmp("SPEED:", token1) != 0)
      {
        write_to_log_and_console(string);
        flag_wrong_line = 1;
        break;
      }
      else if(position == 6)
      {
        if(atoi(token1) <= 0 || atoi(token1) > 360)
        {
          write_to_log_and_console(string);
          sprintf(string, "CAR'S %s SPEED MUST BE A NUMBER BETWEEN 1 AND 360 AND NOT %s\n", car[1], token1);
          write_to_log_and_console(string);
          flag_wrong_line = 1;
          break;
        }
        else{
          leave_numeric_chars(token1);
          strcpy(car[car_index++], token1);
        }
      }
      //CONSUMPTION
      else if(position == 7 && strcmp("CONSUMPTION:", token1) != 0)
      {
        write_to_log_and_console(string);
        flag_wrong_line = 1;
        break;
      }
      else if(position == 8)
      {
        if(atof(token1) <= 0 || atof(token1) >= config.deposit_capacity)
        {
          write_to_log_and_console(string);
          sprintf(string, "CAR'S %s CONSUMPTION MUST BE A NUMBER BETWEEN 1 AND MAX DEPOSIT CAPACITY AND NOT %s\n", car[1], token1);
          write_to_log_and_console(string);
          flag_wrong_line = 1;
          break;
        }
      
        else{
          line[strcspn(line, ",")] = 0;
          strcpy(car[car_index++], token1);
        }
      }
      //RELIABILITY
      else if(position == 9 && strcmp("RELIABILITY:", token1) != 0)
      {
        write_to_log_and_console(string);
        flag_wrong_line = 1;
        break;
      }
      else if(position == 10)
      {
        if(atoi(token1) <= 0 || atoi(token1) > 100)
        {
          write_to_log_and_console(string);
          sprintf(string, "CAR'S %s RELIABILITY MUST BE A NUMBER BETWEEN 1 AND 100 AND NOT %s\n", car[1], token1);
          write_to_log_and_console(string);
          flag_wrong_line = 1;
        }
        else
        {
          leave_numeric_chars(token1);
          strcpy(car[car_index++], token1);
        }
      }
      token1 = strtok(NULL, " ");
      position++;
    }

    if(flag_wrong_line == 0)
    { 
      int result = check_if_car_data_exists(config.numTeams, config.numTeams*config.numCars, shared_var->car_numbers, rows, columns, car);
      if(result == 0)
      {
        write_int_SharedMemory(&shared_var->teams_in_race, read_int_SharedMemory(&shared_var->teams_in_race) + add_new_car(rows, columns, car));
        sprintf(string, "NEW CAR LOADED => %s\n", aux);
        write_to_log_and_console(string);
        int index = read_int_SharedMemory(&shared_var->cars_numbers_index);
        write_int_SharedMemory(&shared_var->car_numbers[index], atoi(car[1]));
        write_int_SharedMemory(&shared_var->cars_numbers_index, index + 1);
      }
      else if( result == 1 )
      {
        write_to_log_and_console(string);
        sprintf(string, "TEAM %s IS ALREADY FULL\n", car[0]);
        write_to_log_and_console(string);
      }
      else if( result == 2)
      {
        write_to_log_and_console(string);
        sprintf(string, "CAR'S NUMBER %s ALREADY EXISTS\n", car[1]);
        write_to_log_and_console(string);
      }
    }
  }

  return;
}

/* --------------------------------- AUXILIARY FUNCTIONS ----------------- */
void get_indexes_of_cars_in_race(int *indexes)
{
  int aux = 0;
  for(int i = 0; i < config.numTeams*config.numCars; ++i){
    if(cars[i].is_in_race == 1) indexes[aux++] = i;
  }
}

void clean_indexes_cars(int *indexes)
{
  for(int i = 0; i < config.numTeams * config.numCars; ++i){
    indexes[i] = -1;
  }
}

int get_indexes_of_teams_in_race(int *indexes)
{
  int res, aux = 0;
  for(int i = 0; i < config.numTeams; ++i){
    res = does_team_have_cars(i);
    if(res == 1){
      indexes[aux++] = i;
    }
  }
  return aux;
}

int does_team_have_cars(int team_num)
{
  int is_in_race;
  for (int i = 0; i < config.numCars && cars[team_num*config.numCars + i].car_num != 0; i++)
  {
    is_in_race = read_int_SharedMemory(&cars[team_num*config.numCars + i].is_in_race);
    if(is_in_race == 1){
      return 1;
    }
  }
  return 0;
}

void clean_indexes_teams(int *indexes)
{
  for(int i = 0; i < config.numTeams; ++i){
    indexes[i] = -1;
  }
}

int add_new_car(int rows, int columns, char data[rows][columns])
{
  int sizeCars = config.numCars,
      sizeTeams = config.numTeams;

  for(int i = 0; i < sizeTeams; ++i)
  {
    // Team already exists
    if(strcmp(teams[i].team_name, data[0]) == 0){
      for(int j = 0; j < sizeCars; ++j)
      {
        if( cars[i*sizeCars + j].car_num == 0)
        {
          cars[i*sizeCars + j].car_num = atoi(data[1]);
          cars[i*sizeCars + j].speed = atoi(data[2]);
          cars[i*sizeCars + j].consumption = atof(data[3]);
          cars[i*sizeCars + j].reliability = atoi(data[4]);
          statistic->cars_in_race++;
          return 1;
        }
      }
      return 0;
    }

    // Team doesn't exist - create new
    else if( teams[i].team_name[0] == 0)
    {
      strcpy( teams[i].team_name, data[0]);
      cars[i*sizeCars].car_num = atoi(data[1]);
      cars[i*sizeCars].speed = atoi(data[2]);
      cars[i*sizeCars].consumption = atof(data[3]);
      cars[i*sizeCars].reliability = atoi(data[4]);
      statistic->cars_in_race++;
      return 1;
    }
  }
  return 0;
}

void prepare_cars_for_race(){
  int sizeCars = config.numCars;
  for(int i = 0; i < config.numTeams; ++i){
      teams[i].box_state = FREE;
      for(int j = 0; j < config.numCars; ++j){
        if(cars[i*sizeCars + j].car_num != 0){
          cars[i*sizeCars + j].is_in_race = 1;
        }
        else{
          cars[i*sizeCars + j].is_in_race = 0;
        }
        cars[i*sizeCars + j].remaining_fuel = config.deposit_capacity;
        cars[i*sizeCars + j].needs_fuel = 0;
        cars[i*sizeCars + j].team_number = i;
        cars[i*sizeCars + j].car_state = NORMAL;
        cars[i*sizeCars + j].is_at_0m = 1;
        cars[i*sizeCars + j].has_malfunction = 0;
        cars[i*sizeCars + j].meters_in_race = 0;
        cars[i*sizeCars + j].total_laps = 0;
        cars[i*sizeCars + j].total_box_stops = 0;
        cars[i*sizeCars + j].position = 0;
      }
  }
}

int check_if_car_data_exists(int num_of_teams,
                             int num_of_cars,
                             int *cars_numbers,
                             int rows, 
                             int columns, 
                             char data[rows][columns])
{
  // Check if the team is already full
  int cars_per_team = config.numCars;
  for(int i = 0; i < num_of_teams; ++i)
  {
    if(strcmp(teams[i].team_name, data[0]) == 0)
    {
      if(cars[(i+1)*cars_per_team - 1].car_num != 0)
      {
        return 1;
      }
    }
  }

  // Check if car's number is already taken
  for(int j = 0; j < num_of_cars; ++j)
  {
    if(cars_numbers[j] != 0){
      if(cars_numbers[j] == atoi(data[1]))
      {
        return 2;
      }
    }
  }

  return 0;
}

void print_cars(){
  char string[1000];
  write_to_log_and_console("\n\n");
  for(int i = 0; i < config.numTeams; ++i)
  {
    if(teams[i].team_name[0] != '\0'){
      sprintf(string, "Team: %s\n", teams[i].team_name);
      write_to_log_and_console(string);

      for(int j = 0; j < config.numCars; ++j)
      {
        if( cars[i*config.numCars + j].car_num != 0)
        {
          sprintf(string, "\t[%d] Car: %d  is_in_race: %d  needs_fuel: %d  is_at_0m: %d  has_malfunction: %d  meters_in_race: %.3f  remaining_fuel: %.3f  total_laps: %d  position: %d\n", 
          cars[i*config.numCars + j].car_state,
          cars[i*config.numCars + j].car_num, 
          cars[i*config.numCars + j].is_in_race, 
          cars[i*config.numCars + j].needs_fuel, 
          cars[i*config.numCars + j].is_at_0m,
          cars[i*config.numCars + j].has_malfunction,
          cars[i*config.numCars + j].meters_in_race,
          cars[i*config.numCars + j].remaining_fuel,
          cars[i*config.numCars + j].total_laps,
          cars[i*config.numCars + j].position);
          write_to_log_and_console(string);
        }
      }
    }
  }
}



void print_config(){
  char string[LINE_SIZE];
  sprintf(string, "\ttimeunits: %d  distance: %d  laps: %d  numTeams: %d  numCars: %d  T_avaria: %d  t_Box_min: %d  t_box_max: %d deposit_capacity: %d\n",
  config.timeUnits,
  config.distance,
  config.laps, 
  config.numTeams,
  config.numCars,
  config.T_Avaria,
  config.T_Box_Min,
  config.T_Box_Max,
  config.deposit_capacity);
  write_to_log_and_console(string);
}


void leave_numeric_chars(char *str)
{
  char strStripped[50];
  int i = 0, c = 0;
  for(; i < strlen(str); i++)
  {
    if (isdigit(str[i]))
    {
      strStripped[c] = str[i];
      c++;
    }
  }
  strStripped[c] = '\0';
  strcpy(str, strStripped);
}

void leave_alphanumeric_chars(char *str)
{
  char strStripped[50];
  int i = 0, c = 0;
  for(; i < strlen(str); i++)
  {
    if (isalnum(str[i]))
    {
      strStripped[c] = str[i];
      c++;
    }
  }
  strStripped[c] = '\0';
  strcpy(str, strStripped);
}