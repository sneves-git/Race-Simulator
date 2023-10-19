/*
  Created by:
    Samuel Pires - 2019225195
    Sofia Neves  - 2019220082
*/


#include "projeto.h"

void termination_handler(int signum){
  if(read_int_SharedMemory(&shared_var->is_EINTR_set) == 0){
    if(signum == SIGINT){
        write_to_log_and_console("\nSIGNAL SIGINT RECEIVED\n");

        write_int_SharedMemory(&shared_var->is_EINTR_set, 1);
        end_race_signal();
        print_statistics();

        for(int i = 0; i < 2; i++) wait(NULL); // parent waits for all children
        cleanup();
        exit(0);
    }
    else if(signum == SIGTSTP){
      write_to_log_and_console("\nSIGNAL SIGTSTP RECEIVED\n");
      print_statistics();
    }
  }
}


void end_race_signal()
{
  if(read_int_SharedMemory(&shared_var->has_race_started) == 1){
    pthread_mutex_lock(&shared_var->mutex_start_race);
    while(read_int_SharedMemory(&statistic->cars_in_race) != 0){
      pthread_cond_wait(&shared_var->start_race, &shared_var->mutex_start_race);
    }
    pthread_mutex_unlock(&shared_var->mutex_start_race);
  }
  else{
    write_int_SharedMemory(&shared_var->has_race_started, 1);
    pthread_cond_broadcast(&shared_var->start_race);
  }
}



/* ------------------------------- PRINT STATISTICS ------------------------ */
void insertNodeListFive(struct ListTopCars **root, struct Car car, char teamName[512]) {
  struct ListTopCars *newCar, *previousCar = NULL, *temp = *root;
  int cont = true;
  newCar = (struct ListTopCars *) malloc(sizeof(struct ListTopCars));
  newCar->car = car;
  newCar->next = NULL;
  strcpy(newCar->team_name, teamName);

  while (temp != NULL && cont) {
    if (car.total_laps == temp->car.total_laps) {
        if (car.position == temp->car.position) {
            if(car.meters_in_race == temp->car.meters_in_race)
                cont = false;
              else if(car.meters_in_race < temp->car.meters_in_race)
                cont = false;
              else{
                previousCar = temp;
                temp = temp->next;
              }
        }else if (car.position < temp->car.position)
              cont = false;
        else{
          previousCar = temp;
          temp = temp->next;
        }

    }else if(car.total_laps < temp->car.total_laps)
      cont = false;
    else{
      previousCar = temp;
      temp = temp->next;
    }
  }
    
  newCar->next = temp;
  if (previousCar == NULL) *root = newCar;
  else previousCar->next = newCar;
}

int insertListFive(struct ListTopCars **root) {
  int countCars = 0;
  *root = NULL;
  for(int i = 0; i < config.numTeams; ++i) {
    for (int j = 0; j < config.numCars; ++j) {
      if(cars[i * config.numCars + j].car_num != 0 
        && read_int_SharedMemory(&cars[i * config.numCars + j].car_state) != WITHDRAWAL)
        {
          insertNodeListFive(root, cars[i * config.numCars + j], teams[i].team_name);
          countCars++;
        }
    }
  }
  return countCars;
}

void print_statistics() {
    char string[3000], txt[512];
    struct ListTopCars *firstCar = NULL, *carsTop5 = NULL;
    int countCars;

    string[0] = '\0';

    countCars = insertListFive(&carsTop5);
    firstCar = carsTop5;
    strcat(string, " ---------------------------------- STATISTICS ------------------------- \n");

    for (int i = 1; i <= countCars; i++) {
      txt[0] = '\0';
      if (i <= 5) {
          sprintf(txt, "TOP %d:\n\tCar Number: %d\n\tTeam Name: %s\n\tTotal Laps: %d\n\tTotal Box Stops: %d\n", i, carsTop5->car.car_num, carsTop5->team_name, carsTop5->car.total_laps, carsTop5->car.total_box_stops);
          strcat(string, txt);
      }

      if(i == countCars)
      {
        sprintf(txt, "Last Car:\n\tCar Number: %d\n\tTeam Name: %s\n\tTotal Laps: %d\n\tTotal Box Stops: %d\n", carsTop5->car.car_num, carsTop5->team_name, carsTop5->car.total_laps, carsTop5->car.total_box_stops);
        strcat(string, txt);
      }

      carsTop5 = carsTop5->next;
    }

    if(countCars == 0){
      sprintf(txt, "\tNO STATISTICS OF RACE YET OR ALL CARS WITHDRAWELLED FROM RACE\n");
      strcat(string, txt);
    }
    sprintf(txt, "\n\tTotal of Malfunction: %d\n\tTotal of Fills: %d\n\tCars in Race: %d\n", statistic->total_of_malfunctions, statistic->total_of_fills, statistic->cars_in_race);
    strcat(string, txt);

    write_to_log_and_console(string);

    //free list of all cars
    if(countCars != 0)
      while(firstCar->next != NULL)
      {
          struct ListTopCars *aux = firstCar->next;
          free(firstCar);
          firstCar = aux;
      }
    free(firstCar);
}