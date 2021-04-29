all : etap6c etap6s
CC = gcc
CFLAGS = -std=gnu99 -Wall 

etap6c: etap6c.c
	${CC} -o etap6c etap6c.c ${CFLAGS}

etap6s: etap6s.c
	${CC} -o etap6s etap6s.c ${CFLAGS}

.PHONY : clean 
clean:
	rm etap6c etap6s