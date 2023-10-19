/*
  Created by:
    Samuel Pires - 2019225195
    Sofia Neves  - 2019220082
*/
#include "projeto.h"


void team_manager(struct Team *team, int team_num)
{
  new_action.sa_handler = SIG_IGN;
  sigaction(SIGINT,&new_action,NULL);
  sigaction(SIGTSTP,&new_action,NULL);

  pthread_t *thread_cars = (pthread_t *) malloc(sizeof(pthread_t) * config.numCars);

  int car_state, is_at_0m, needs_fuel, has_malfuntion, is_in_race;
  char string[500];


  // Create car threads
  for (int i = team_num*config.numCars; i < team_num*config.numCars + config.numCars && cars[i].car_num != 0; i++) //<---------- ver se esta bem, serve para criar somente as threads que sÃ£o precisas
  { 
    pthread_create(&thread_cars[i], NULL, car, &cars[i]);
  }


  int box_state, remaining_time, index_car_in_repair;
  while(true)
  {
    sem_wait(&team->mutex_start_team);
 

    if(read_int_SharedMemory(&statistic->cars_in_race) == 0){
      break;
    }

    // If box occupied, then decrement time until 0
    box_state = read_int_SharedMemory(&team->box_state);
    if(box_state == OCCUPIED){

      remaining_time = remaining_time - config.timeUnits;
      if(remaining_time <= 0)
      {
        write_int_SharedMemory(&cars[index_car_in_repair].car_state, NORMAL);
        write_int_SharedMemory(&cars[index_car_in_repair].has_malfunction, 0);
        write_int_SharedMemory(&team->box_state, FREE);
        sprintf(string, "STATE NORMAL ACTIVATED IN CAR %d\n", cars[index_car_in_repair].car_num);
        write(cars[index_car_in_repair].fd[1], &string, sizeof(string));
        

        needs_fuel = read_int_SharedMemory(&cars[index_car_in_repair].needs_fuel);
        if(needs_fuel == 1)
        {
          write_int_SharedMemory(&cars[index_car_in_repair].needs_fuel, 0);
          write_float_SharedMemory(&cars[index_car_in_repair].remaining_fuel, config.deposit_capacity);
          write_int_SharedMemory(&statistic->total_of_fills, read_int_SharedMemory(&statistic->total_of_fills) + 1);
        }
        // -------------------NOTIFY UNNAMED PIPE

        // Update box state
        for (int i = 0; i < config.numCars && cars[team_num*config.numCars + i].car_num != 0; i++)
        {
          car_state = read_int_SharedMemory(&cars[team_num*config.numCars + i].car_state);
          is_in_race = read_int_SharedMemory(&cars[team_num*config.numCars + i].is_in_race);
          if(car_state == SECURITY && is_in_race == 1){
            write_int_SharedMemory(&team->box_state, RESERVED);
            break;
          }
        }
        sem_post(&team->mutex_finish_team);
        continue;
      }
      else
      {
        sem_post(&team->mutex_finish_team);
        continue;
      }
    }

    // Update box state
    if(box_state != RESERVED && box_state != OCCUPIED){
      for (int i = 0; i < config.numCars && cars[team_num*config.numCars + i].car_num != 0; i++)
      {
        car_state = read_int_SharedMemory(&cars[team_num*config.numCars + i].car_state);
        is_in_race = read_int_SharedMemory(&cars[team_num*config.numCars + i].is_in_race);
        if(car_state == SECURITY && is_in_race == 1){
          write_int_SharedMemory(&team->box_state, RESERVED);
          break;
        }
      }
    }

    // Check car that needs box
    for (int i = 0; i < config.numCars && cars[team_num*config.numCars + i].car_num != 0; i++)
    {
      car_state = read_int_SharedMemory(&cars[team_num*config.numCars + i].car_state);
      is_at_0m = read_int_SharedMemory(&cars[team_num*config.numCars + i].is_at_0m);
      has_malfuntion = read_int_SharedMemory(&cars[team_num*config.numCars + i].has_malfunction);
      is_in_race = read_int_SharedMemory(&cars[team_num*config.numCars + i].is_in_race);
      needs_fuel = read_int_SharedMemory(&cars[team_num*config.numCars + i].needs_fuel);
      is_at_0m = read_int_SharedMemory(&cars[team_num*config.numCars + i].is_at_0m);
      box_state = read_int_SharedMemory(&team->box_state);

      if(box_state == OCCUPIED){
        break;
      }
      else if(box_state == RESERVED) {
        if(car_state == SECURITY
            && is_in_race == 1
            && is_at_0m == 1)
        {

          if(has_malfuntion){
            remaining_time = (rand() % (config.T_Box_Max - config.T_Box_Min + 1)) + config.T_Box_Min;
          }else{
            remaining_time = FUEL_FILL_TIME;
          }

          index_car_in_repair = team_num*config.numCars + i;
          write_int_SharedMemory(&cars[team_num*config.numCars + i].total_box_stops, read_int_SharedMemory(&cars[team_num*config.numCars + i].total_box_stops) + 1);
          write_int_SharedMemory(&team->box_state, OCCUPIED);
          write_float_SharedMemory(&cars[team_num*config.numCars + i].meters_in_race, 0);
          write_int_SharedMemory(&cars[team_num*config.numCars + i].car_state, BOX);

          sprintf(string, "STATE BOX ACTIVATED IN CAR %d\n", cars[team_num*config.numCars + i].car_num);
          write(cars[team_num*config.numCars + i].fd[1], &string, sizeof(string));
          break;
        }
      }

      else if(box_state == FREE)
      {
        if(needs_fuel == 1
          && is_in_race == 1
          && is_at_0m == 1)
        {
          index_car_in_repair = team_num*config.numCars + i;
          remaining_time = FUEL_FILL_TIME;
          
          write_int_SharedMemory(&cars[team_num*config.numCars + i].total_box_stops, read_int_SharedMemory(&cars[team_num*config.numCars + i].total_box_stops) + 1);
          write_int_SharedMemory(&team->box_state, OCCUPIED);
          write_float_SharedMemory(&cars[team_num*config.numCars + i].meters_in_race, 0);
          write_int_SharedMemory(&cars[team_num*config.numCars + i].car_state, BOX);

          sprintf(string, "STATE BOX ACTIVATED IN CAR %d\n", cars[team_num*config.numCars + i].car_num);
          write(cars[team_num*config.numCars + i].fd[1], &string, sizeof(string));
          break;
        }
      }
    }

    sem_post(&team->mutex_finish_team);
  }

  // Wait for all car threads to finish
  for (int i = team_num*config.numCars; i < team_num*config.numCars + config.numCars && cars[i].car_num != 0; i++){
    pthread_join(thread_cars[i], NULL); 
  }

}


