CC = gcc
CFLAGS = -Wall
TARGET = hw02
SRC = hw02.c
OBJ = $(SRC:.c=.o)
FIFOS = /tmp/fifo1 /tmp/fifo2

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET) $(FIFOS)