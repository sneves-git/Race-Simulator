/*
  Created by:
    Samuel Pires - 2019225195
    Sofia Neves  - 2019220082
*/


#include "projeto.h"

void *car(void *void_id)
{
  struct Car *car = (struct Car *)void_id;
  struct malfunction_message received_msg;
  int state, laps, total_laps, has_malfunction;
  float remaining_fuel, meters, fuel, meters_in_race, one_laps_fuel;

  char string[LINE_SIZE];

  int time = 0;
  sem_post(&car->mutex_finish_car);

  while(true){
    sem_wait(&car->mutex_start_car);
  
    has_malfunction = read_int_SharedMemory(&car->has_malfunction);

    time += config.timeUnits;
    if(has_malfunction == 0 && time >= config.T_Avaria)
    {
      msgrcv(mqid, &received_msg, sizeof(struct malfunction_message), car->car_num, 0);
      if(received_msg.malfunction_occurred == 1){
        write_int_SharedMemory(&car->car_state, SECURITY);
        write_int_SharedMemory(&car->has_malfunction, 1);

        sprintf(string, "STATE SECURITY ACTIVATED IN CAR %d\n", car->car_num);
        write(car->fd[1], &string, sizeof(string));
      }
      time = 0;
    }
    else if(has_malfunction == 1 && time >= config.T_Avaria)
    {
      time = 0;
    }


    state = read_int_SharedMemory(&car->car_state);
    if(state == BOX){
      sem_post(&car->mutex_finish_car);
      continue;
    }


    state = read_int_SharedMemory(&car->car_state);
    fuel = read_float_SharedMemory(&car->remaining_fuel);
    meters_in_race = read_float_SharedMemory(&car->meters_in_race);
    if(state == NORMAL)
    {
      remaining_fuel = fuel - car->consumption*config.timeUnits;
      meters = meters_in_race + car->speed*config.timeUnits;
      one_laps_fuel = (config.distance/(car->speed*config.timeUnits))*car->consumption*config.timeUnits;

      if(remaining_fuel < one_laps_fuel*4
        && remaining_fuel > one_laps_fuel*2)
      {
        write_int_SharedMemory(&car->needs_fuel, 1);
      }
      else if(remaining_fuel <= one_laps_fuel*2
                && remaining_fuel > 0)
      {
        write_int_SharedMemory(&car->needs_fuel, 1);
        write_int_SharedMemory(&car->car_state, SECURITY);

        sprintf(string, "STATE SECURITY ACTIVATED IN CAR %d\n", car->car_num);
        write(car->fd[1], &string, sizeof(string));
      } 
    }
    else if(state == SECURITY)
    {
      remaining_fuel = fuel - 0.4*car->consumption*config.timeUnits;
      meters = meters_in_race + 0.3*car->speed*config.timeUnits;
    }

    if(remaining_fuel <= 0){
      write_int_SharedMemory(&car->car_state, WITHDRAWAL);
      write_int_SharedMemory(&statistic->cars_in_race, read_int_SharedMemory(&statistic->cars_in_race)-1);
      write_int_SharedMemory(&car->is_in_race, 0);

      sprintf(string, "STATE WITHDRAWAL ACTIVATED IN CAR %d\n", car->car_num);
      write(car->fd[1], &string, sizeof(string));

      // Car withdrawalled but is last in race and broadcasts end of race
      if(read_int_SharedMemory(&statistic->cars_in_race) == 0){
        pthread_cond_broadcast(&shared_var->finish_race);
        sem_post(&car->mutex_finish_car);
        break;
      }
      // Car withdrawalled and isn't last in race so stops
      else
      {
        sem_post(&car->mutex_finish_car);
        pthread_mutex_lock(&shared_var->mutex_finish_race);
        while(read_int_SharedMemory(&statistic->cars_in_race) != 0){
          pthread_cond_wait(&shared_var->finish_race, &shared_var->mutex_finish_race);
        }
        pthread_mutex_unlock(&shared_var->mutex_finish_race);
        break;
      }
    }
    else
    {
      write_float_SharedMemory(&car->remaining_fuel, remaining_fuel);
    }


    laps = floor(meters / config.distance);
    state = read_int_SharedMemory(&car->car_state);
    total_laps = read_int_SharedMemory(&car->total_laps);
    // At least one lap occurred
    if( laps >= 1 )
    {

      write_int_SharedMemory(&car->total_laps, total_laps+laps);
      if(total_laps + laps >= config.laps){
        write_int_SharedMemory(&car->car_state, FINISHED);
        write_int_SharedMemory(&statistic->cars_in_race, read_int_SharedMemory(&statistic->cars_in_race)-1);
        write_int_SharedMemory(&car->position, read_int_SharedMemory(&statistic->position)+1);
        write_int_SharedMemory(&statistic->position, read_int_SharedMemory(&statistic->position)+1);
        write_float_SharedMemory(&car->meters_in_race, 0);
        write_int_SharedMemory(&car->is_at_0m, 1);
        write_int_SharedMemory(&car->is_in_race, 0);

        sprintf(string, "STATE FINISHED ACTIVATED IN CAR %d\n", car->car_num);
        write(car->fd[1], &string, sizeof(string));

        if(read_int_SharedMemory(&statistic->cars_in_race) == 0){
          pthread_cond_broadcast(&shared_var->finish_race);
          sem_post(&car->mutex_finish_car);
          break;
        }
        else
        {
          sem_post(&car->mutex_finish_car);
          pthread_mutex_lock(&shared_var->mutex_finish_race);
          while(read_int_SharedMemory(&statistic->cars_in_race) != 0){
            pthread_cond_wait(&shared_var->finish_race, &shared_var->mutex_finish_race);
          }
          pthread_mutex_unlock(&shared_var->mutex_finish_race);
          break;
        }
      }
      else if(read_int_SharedMemory(&shared_var->is_EINTR_set) == 1){
        write_int_SharedMemory(&car->car_state, FINISHED);
        write_int_SharedMemory(&statistic->cars_in_race, read_int_SharedMemory(&statistic->cars_in_race)-1);
        write_int_SharedMemory(&car->position, read_int_SharedMemory(&statistic->position)+1);
        write_int_SharedMemory(&statistic->position, read_int_SharedMemory(&statistic->position)+1);
        write_float_SharedMemory(&car->meters_in_race, 0);
        write_int_SharedMemory(&car->is_at_0m, 1);
        write_int_SharedMemory(&car->is_in_race, 0);

        sprintf(string, "STATE FINISHED ACTIVATED IN CAR %d\n", car->car_num);
        write(car->fd[1], &string, sizeof(string));

        if(read_int_SharedMemory(&statistic->cars_in_race) == 0){
          pthread_cond_broadcast(&shared_var->finish_race);
          sem_post(&car->mutex_finish_car);
          break;
        }
        else
        {
          sem_post(&car->mutex_finish_car);
          pthread_mutex_lock(&shared_var->mutex_finish_race);
          while(read_int_SharedMemory(&statistic->cars_in_race) != 0){
            pthread_cond_wait(&shared_var->finish_race, &shared_var->mutex_finish_race);
          }
          pthread_mutex_unlock(&shared_var->mutex_finish_race);
          break;
        }
      }
      else{
        write_float_SharedMemory(&car->meters_in_race, fmod(meters, config.distance));
        write_int_SharedMemory(&car->is_at_0m, 1);
      }
    }
    // No laps 
    else
    { 
      write_float_SharedMemory(&car->meters_in_race, meters);
      write_int_SharedMemory(&car->is_at_0m, 0);
    }

    sem_post(&car->mutex_finish_car);
  }

  pthread_exit(NULL);
}