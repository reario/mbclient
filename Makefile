SUFFIXES : .o.c

CC = gcc
# librerie e include per modbus
INCDIR = /usr/local/include
LIBDIR = /usr/local/lib

INCDIRPQ = /usr/local/pgsql/include/
LIBDIRPQ = /usr/local/pgsql/lib/

all: s s1

s: s.o
	$(CC) -Wall -L${LIBDIRPQ} -L${LIBDIR} -lpq -lmodbus -lm $^ -o $@

s1: s1.o
	$(CC) -Wall -L${LIBDIRPQ} -L${LIBDIR} -lpq -lmodbus -lm $^ -o $@


# vengono costruiti fli object
.c.o: gh.h
	$(CC) -c -g -Wall -DDOINSERT3 -I${INCDIRPQ} -I$(INCDIR)/modbus $< -o $@

# cancella i file non necessari e pulisce la directory, pronta per una compilazione pulita
clean :
	rm -f *~ *.o *.i *.s *.core s s1
