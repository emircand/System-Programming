all: parking_lot

CC = gcc
CFLAGS = -Wall -pthread

parking_lot: parking_lot.c
	$(CC) parking_lot.c -o parking_lot $(CFLAGS)

clean:
	-rm -f parking_lot