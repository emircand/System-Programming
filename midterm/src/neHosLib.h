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

#define SERVER_FIFO_TEMP "/tmp/server.%d.fifo"
#define CLIENT_FIFO_TEMP "/tmp/client.%d.fifo"

#define ERR_MSG_LEN 128

#define LOG_FILE_TEMPLATE "nehoslog.%d.log"

#define LOG_FILE_LEN (sizeof(LOG_FILE_TEMPLATE) + 20)

#define REG_SERVER_FIFO_TEMPLATE "/tmp/nehos_reg_sv.%d"

#define REQ_SERVER_FIFO_TEMPLATE "/tmp/nehos_req_sv.%d"

#define CLIENT_FIFO_TEMPLATE "/tmp/nehos_cl.%d"

#define REG_SERVER_FIFO_NAME_LEN (sizeof(REG_SERVER_FIFO_TEMPLATE) + 20)

#define REQ_SERVER_FIFO_NAME_LEN (sizeof(REQ_SERVER_FIFO_TEMPLATE) + 20)

#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE) + 20)

#define CMD_LEN 64

#define CMD_ARG_MAX 32

#define BUF_SIZE 256

#define CWD_SIZE 256

#define LOG_LEN 512

enum req_cmd {HELP, LIST, READ_F, WRITE_T, UPLOAD, DOWNLOAD, QUIT, KILL_SERVER};

enum resp_status {RESP_OK, RESP_ERROR, RESP_PART, RESP_CONT, RESP_APPROVE, RESP_CONNECT, RESP_DISCONNECT};

struct request {
    pid_t pid;  // Client PID
    char data[BUF_SIZE];  // Request data
};

struct queue {
    int front;
    int rear;
    int capacity;
    int size;
    struct request **elements;
};

struct pid_list {
    int size;
    int capacity;
    pid_t *pids;
};

#endif