CC = gcc
CFLAGS = -Wall
TARGET = hw01
SRC = hw01.c addStudent.c searchStudent.c sortList.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c hw01.h addStudent.h searchStudent.h sortList.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)