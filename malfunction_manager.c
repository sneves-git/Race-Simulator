/*
  Created by:
    Samuel Pires - 2019225195
    Sofia Neves  - 2019220082
*/
#include "projeto.h"


void malfunction_manager()
{
  signal(SIGINT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);

  // Intializes random number generator
  time_t t;
  srand((unsigned) time(&t));

  int cars_in_race = read_int_SharedMemory(&statistic->cars_in_race), has_malfunction;
  struct malfunction_message *sent_msg = malloc(sizeof(struct malfunction_message) * cars_in_race);
  char string[LINE_SIZE];

  // See if race has started
  pthread_mutex_lock(&shared_var->mutex_start_race);
  while(read_int_SharedMemory(&shared_var->has_race_started) != 1){
    pthread_cond_wait(&shared_var->start_race, &shared_var->mutex_start_race);
  }
  pthread_mutex_unlock(&shared_var->mutex_start_race);

  if(read_int_SharedMemory(&shared_var->is_EINTR_set) == 1){
    exit(0);
  }

  int time = 0, num_cars = config.numTeams * config.numCars;
  while(true){

    // See if T_avaria seconds passed
    pthread_mutex_lock(&shared_var->mutex_malfunction_man);
    while(time != config.T_Avaria){
      pthread_cond_wait(&shared_var->malfunction_man, &shared_var->mutex_malfunction_man);
      time = config.T_Avaria;
    }
    time = 0;
    pthread_mutex_unlock(&shared_var->mutex_malfunction_man);

    if(read_int_SharedMemory(&statistic->cars_in_race) == 0) break;

    // Create new malfunction (or not) for cars in race
    cars_in_race = read_int_SharedMemory(&statistic->cars_in_race);
    for(int i = 0; i < num_cars; ++i)
    {
      if(read_int_SharedMemory(&cars[i].is_in_race) == 1
        && read_int_SharedMemory(&cars[i].has_malfunction) != 1)
      {
        sent_msg[i].mtype = cars[i].car_num;

        has_malfunction = rand() % 101;
        if(has_malfunction > cars[i].reliability){
          sent_msg[i].malfunction_occurred = 1;
          write_int_SharedMemory(&statistic->total_of_malfunctions, read_int_SharedMemory(&statistic->total_of_malfunctions) + 1);

          sprintf(string, "NEW PROBLEM IN CAR %d\n", cars[i].car_num);
          write_to_log_and_console(string);
        }else{
          sent_msg[i].malfunction_occurred = 0;
        }
        
        msgsnd(mqid, &sent_msg[i], sizeof(struct malfunction_message), 0);
      }
    }

    sem_post(go);
  }
}