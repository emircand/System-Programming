CC = gcc
CFLAGS = -Wall -pthread -std=gnu99

all: MWCp

MWCp: main.o manager.o worker.o buffer.o
	$(CC) $(CFLAGS) -o MWCp main.o manager.o worker.o buffer.o

main.o: main.c utils.h
	$(CC) $(CFLAGS) -c main.c

manager.o: manager.c utils.h
	$(CC) $(CFLAGS) -c manager.c

worker.o: worker.c utils.h
	$(CC) $(CFLAGS) -c worker.c

buffer.o: buffer.c utils.h
	$(CC) $(CFLAGS) -c buffer.c

clean:
	rm -f *.o MWCp
