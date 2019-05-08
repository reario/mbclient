SUFFIXES : .o.c

CC = gcc
# librerie e include per modbus
INCDIR = /home/reario/include
LIBDIR = /home/reario/lib

INCDIRPQ = /usr/local/pgsql/include/
LIBDIRPQ = /usr/local/pgsql/lib/

all: mbclient t

t: t.o
	$(CC) -Wall -L${LIBDIRPQ} -L${LIBDIR} -lpq -lpthread -lrt -lmodbus -lm $^ -o $@

mbclient: mbclient.o 
	$(CC) -Wall -L${LIBDIRPQ} -L${LIBDIR} -lpq -lrt -lpthread -lmodbus -lm $^ -o $@

# vengono costruiti file object
.c.o: gh.h
	$(CC) -c -g -Wall -DDOINSERT3 -I${INCDIRPQ} -I$(INCDIR)/modbus $< -o $@

# cancella i file non necessari e pulisce la directory, pronta per una compilazione pulita
clean :
	rm -f *~ *.o *.i *.s *.core mbclient
