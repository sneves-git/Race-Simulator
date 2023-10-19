/*
  Created by:
    Samuel Pires - 2019225195
    Sofia Neves  - 2019220082
*/


#ifndef _PROJETO_H_
#define _PROJETO_H_

//i/ compile as gcc -pthread -Wall process_tasks.c -o ptasks
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
//Fork and Wait and Shared memory
#include <sys/types.h>
#include <unistd.h>
//Thread
#include <pthread.h>
//Error
#include <errno.h>
//Shared memory
#include <sys/shm.h>
//Time
#include <time.h>
//Named pipe
#include <string.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
//Boolean
#include <stdbool.h>
//Semaphore
#include <semaphore.h>
//Don't know 
#include <sys/ipc.h>
//Wait
#include <sys/wait.h>
#include <malloc.h>
//Signals
#include <signal.h>
//Fmod
#include <math.h>
//Message Queue
#include <sys/msg.h>
//Conditional Variables
#include <pthread.h>




typedef int boolean;


//------------------CONSTANT DEFINITIONS-----------------------------------------

#define LINE_SIZE 256

//PROJETO.C (MAIN)
#define EXIT_FAILURE 1

//RACE.C

/* Box states */
#define FREE 0
#define OCCUPIED 1
#define RESERVED 2

#define FUEL_FILL_TIME 2

/* Car states */
#define NORMAL 0
#define SECURITY 1
#define BOX 2
#define WITHDRAWAL 3
#define FINISHED 4

/* Number of cars at the top of race */
#define TOP_GRID 5

/* Named pipe name */
#define NAMED_PIPE_NAME "PIPE"
//------------------Global Variables-----------------------------------------



// PROJETO.C MAIN
/* Time */
time_t now;
struct tm *tm_struct;

/* Files */
FILE *log_file;


/* Named Semaphores */
sem_t *print, 
      *stop_writers,
      *mutex,
      *read_shared_var, 
      *go;


/* Named Pipe */
int named_pipe;


/* Shared Memory */
int shmid;
struct SharedMemory *shared_var;
struct Team *teams;
struct Statistics *statistic;
struct Car *cars;

/* Not in Shared Memory */
struct Config config;


/* Message Queue */
int mqid;


/* Conditional Variable */
pthread_mutexattr_t mutex_signal_start_race;
pthread_condattr_t attrcondv_signal_start_race;

pthread_mutexattr_t mutex_signal_finish_race;
pthread_condattr_t attrcondv_signal_finish_race;

pthread_mutexattr_t mutex_signal_malfunction_man;
pthread_condattr_t attrcondv_signal_malfunction_man;


/* Signals */
struct sigaction new_action;
sigset_t block_set;


//------------------Structures-----------------------------------------

/* Configurations Structure */
struct Config{
    int timeUnits,
        distance,
        laps,
        numTeams, 
        numCars,
        T_Avaria,
        T_Box_Min,
        T_Box_Max,
        deposit_capacity;
};

/* Car structure */
struct Car{
    pthread_t car_thread;

    /* Unnamed Pipe file descriptor*/
    int fd[2];


    /* Unnamed Semaphores */
    sem_t mutex_start_car,
          mutex_finish_car;

    /* Variables needed for race */
    int is_in_race,
        needs_fuel,
        is_at_0m,
        has_malfunction;
    float meters_in_race,
          remaining_fuel;
    
    /* Car characteristics */
    int speed,
        reliability,
        car_state,
        team_number;
    float consumption;

    /* Statistics */
    int car_num,
        total_laps,
        total_box_stops,
        position;
};


/* Statistics structure */
struct Statistics{
    struct Car top_five[5];
    struct Car last_place;
    int total_of_malfunctions,
        total_of_fills,
        cars_in_race,
        position;       
};

/* Team structure */
struct Team{
    char team_name[50];
    int box_state;

    /* Unnamed Semaphores */
    sem_t mutex_start_team,
          mutex_finish_team;
};


/* Shared Memory structure for shared variables needed */
struct SharedMemory{
    int thread_index,
        cars_numbers_index,
        teams_in_race,
        has_race_started,
        is_EINTR_set;
    int *car_numbers;


    /* Number of readers reading shared memory */
    int n_readers;

    /* Conditional variables */
    pthread_mutex_t mutex_start_race;
    pthread_mutex_t mutex_finish_race;
    pthread_mutex_t mutex_malfunction_man;

    pthread_cond_t start_race;
    pthread_cond_t finish_race;
    pthread_cond_t malfunction_man;
};


/* Message structure */
struct malfunction_message{
    // Message type
    long mtype;
    // Message
    int malfunction_occurred;
};


/* Top 5 cars in race structure */
struct ListTopCars{
    char team_name[100];
    struct Car car;
    struct ListTopCars *next;
};


//------------------Functions-----------------------------------------


void race_manager();
void team_manager(struct Team*, int);
void *car(void *);
void malfunction_manager();
void print_cars();
int does_team_have_cars(int);
void get_indexes_of_cars_in_race(int *);
void clean_indexes_cars(int *);
int get_indexes_of_teams_in_race(int *);
void clean_indexes_teams(int *);
void *pipe_manager(void *);
void print_config();

// projeto.c (MAIN)

void initialize();
void prepare_cars_for_race();
void leave_alphanumeric_chars(char *);
void leave_numeric_chars(char *);
void write_to_log_and_console(char *);
int check_if_digit(char *);
int check_config_file_data(FILE *);
void separate_line(char *, int line_size, int list[line_size]);
void cleanup();
void print_results(int, int);
int check_car_file_data(FILE *);
int check_if_car_data_exists(int,
                             int num_of_cars,
                             int cars_numbers[num_of_cars],
                             int rows, 
                             int columns, 
                             char data[rows][columns]);
int add_new_car(int rows, int columns, char data[rows][columns]);
int read_int_SharedMemory(int *);
float read_float_SharedMemory(float *);
void write_float_SharedMemory(float *, float);
void write_int_SharedMemory(int *, int);

int insertListFive(struct ListTopCars **);
void print_statistics();
void insertNodeListFive(struct ListTopCars **, struct Car , char teamName[512]);

//void read_car_file_data(FILE *);
void check_named_pipe_data(char line[LINE_SIZE]);


void termination_handler(int signum);
void end_race_signal();


#endif