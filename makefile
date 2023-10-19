CC          = gcc
FLAGS       = -Wall -g
THREAD		= -pthread -D_REENTRANT
MATH		= -lm
OBJS        = main.o signal_handlers.o race_manager.o team_manager.o car_threads.o malfunction_manager.o
PROG        = car_race2_0

# GENERIC
all:		${PROG}

clean:
			rm ${OBJS} *~ ${PROG}

${PROG}:    ${OBJS}
			${CC} ${FLAGS} ${THREAD} ${OBJS} -o $@ ${MATH}
.c.o:
			${CC} ${FLAGS} ${THREAD} $< -c -o $@ ${MATH}

################
main.o: main.c projeto.h
signal_handlers.o: signal_handlers.c projeto.h

race_manager.o: race_manager.c projeto.h
team_manager.o: team_manager.c projeto.h
car_threads.o: car_threads.c projeto.h

malfunction_manager.o: malfunction_manager.c projeto.h