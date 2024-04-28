#ifndef NEHOSLIB_H
#define NEHOSLIB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>

#define SERVER_FIFO_TEMP "/tmp/server.%d.fifo"
#define CLIENT_FIFO_TEMP "/tmp/client.%d.fifo"

#define ERR_MSG_LEN 128

#define LOG_FILE_TEMPLATE "nehoslog.%d.log"

#define LOG_FILE_LEN (sizeof(LOG_FILE_TEMPLATE) + 20)

#define REG_SERVER_FIFO_NAME_LEN (sizeof(REG_SERVER_FIFO_TEMPLATE) + 20)

#define REQ_SERVER_FIFO_NAME_LEN (sizeof(REQ_SERVER_FIFO_TEMPLATE) + 20)

#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE) + 20)

#define CMD_LEN 64

#define CMD_ARG_MAX 32

#define BUF_SIZE 256

#define CWD_SIZE 256

#define LOG_LEN 512

#define MAX_QUEUE_SIZE 100

enum req_cmd {HELP, LIST, READ_F, WRITE_T, UPLOAD, DOWNLOAD, QUIT, KILL_SERVER};

enum resp_status {RESP_OK, RESP_ERROR, RESP_CONNECT, RESP_DISCONNECT};

enum req_type {CONNECT, COMMAND, TRY_CONNECT};

struct request {
    pid_t pid;  // Client PID
    char data[BUF_SIZE];  // Request data
    enum req_cmd cmd; // Request command
    enum req_type type; // Request type
    int client_number; // Client index
};

typedef struct {
    sem_t sem;
    int child_count;
} SharedData;


struct response {
    int status;
    char data[BUF_SIZE];
};

struct Queue {
    int front, rear, size;
    unsigned capacity;
    struct request** array;
};

struct pid_list {
    int size;
    int capacity;
    pid_t *pids;
};

void sig_handler(int signum);
void err_exit(const char *err);
void handle_client_request(struct request* req,  int server_fifo_fd);
void handle_command(char* response, const char* command); 
struct Queue* createQueue(unsigned capacity);
int isFull(struct Queue* queue);
int isEmpty(struct Queue* queue);
void enqueue(struct Queue* queue, struct request* item);
struct request* dequeue(struct Queue* queue);
void send_connect_response(pid_t client_pid, bool wait); 

#endif