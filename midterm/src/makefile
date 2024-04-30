CC = gcc
CFLAGS = -Wall
TARGET_SERVER = neHosServer
TARGET_CLIENT = neHosClient
SRC_SERVER = neHosServer.c
SRC_CLIENT = neHosClient.c
SRC_QUEUE = queue.c
OBJ_SERVER = $(SRC_SERVER:.c=.o)
OBJ_CLIENT = $(SRC_CLIENT:.c=.o)
OBJ_QUEUE = $(SRC_QUEUE:.c=.o)

.PHONY: all clean run

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(OBJ_SERVER) $(OBJ_QUEUE)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET_CLIENT): $(OBJ_CLIENT) $(OBJ_QUEUE)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET_SERVER) $(TARGET_CLIENT)
	./$(TARGET_SERVER) &
	./$(TARGET_CLIENT)

clean:
	rm -f $(OBJ_SERVER) $(TARGET_SERVER) $(OBJ_CLIENT) $(TARGET_CLIENT) $(OBJ_QUEUE)