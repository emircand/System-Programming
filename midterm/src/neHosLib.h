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

#define CWD_SIZE 256

#define LOG_LEN 512

#define MAX_QUEUE_SIZE 100

#define BUF_SIZE 1024
#define MAX_FILES 100
#define MAX_LINE_LEN 1024
#define SHM_NAME "/shm_ecd"
#define SEM_NAME "/sem"
#define SHM_SIZE 4096
#define PATH_MAX 4096

enum req_cmd {HELP, LIST, READ_F, WRITE_T, UPLOAD, DOWNLOAD, QUIT, KILL_SERVER};

enum resp_status {RESP_OK, RESP_ERROR, RESP_CONNECT, RESP_DISCONNECT, RESP_QUIT};

enum req_type {CONNECT, COMMAND, TRY_CONNECT};

typedef struct {
    sem_t sem, file_sem, queue_sem;
    int child_count;
} SharedData;

struct request {
    pid_t pid;  // Client PID
    char data[BUF_SIZE];  // Request data
    enum req_cmd cmd; // Request command
    enum req_type type; // Request type
    int client_number; // Client index
};




struct response {
    int status;
    char data[BUF_SIZE];
};

typedef struct {
    char cmd[CMD_LEN];
    char args[CMD_ARG_MAX][BUF_SIZE];
    char file_name[BUF_SIZE];
    int arg_count;
    enum req_cmd cmd_id;
} Command;

//Server functions
void sig_handler(int signum);
void err_exit(const char *err);
void handle_client_request(struct request* req,  int server_fifo_fd, DIR* dir);
struct response handle_command(const char* command, DIR* dir);
struct response cmd_list(int client_fd, DIR *server_dir); 
void write_log(const char* message);
void handle_help(const char* command, struct response* resp);
struct response handle_command(const char* command, DIR* dir);
void parse_command(char* request, Command* cmd);
char* readF(const char *filename, DIR *server_dir);
char* readF_Line(const char *filename, int line_num);
void handle_readF(char* cmd_args[], struct response* resp, int num_args); 

//Client functions
void send_connect_response(pid_t client_pid, bool wait); 
struct response handle_response(int client_fd);
void interact_with_server(int server_fd, const char* client_fifo, enum req_type type);
void clean_client();
void connect_to_server(int server_fd, bool wait);


#endif
