CC = gcc
CFLAGS = -Wall -pthread -std=gnu99

all: MWCp

MWCp: 1901042674_main.o manager.o worker.o buffer.o
	$(CC) $(CFLAGS) -o MWCp 1901042674_main.o manager.o worker.o buffer.o

1901042674_main.o: 1901042674_main.c utils.h
	$(CC) $(CFLAGS) -c 1901042674_main.c

manager.o: manager.c utils.h
	$(CC) $(CFLAGS) -c manager.c

worker.o: worker.c utils.h
	$(CC) $(CFLAGS) -c worker.c

buffer.o: buffer.c utils.h
	$(CC) $(CFLAGS) -c buffer.c

clean:
	rm -f *.o MWCp
