IDIR =.
CC=gcc
CFLAGS=-I$(IDIR)

ODIR=../obj
LDIR =../lib

LIBS=-l pthread -l dl -l wiringPi -l sqlite3

_DEPS = aGhSensors.h aGhActuators.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = aGhControl.o aGhSensors.o aGhActuators.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

aGreenHouse: $(OBJ)
	gcc -o ../out/$@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 