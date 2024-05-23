#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

// Structure for buffer
typedef struct {
    int src_fd;
    int dst_fd;
    char src_filename[256];
    char dst_filename[256];
} file_task_t;

typedef struct {
    file_task_t *tasks;
    int capacity;
    int count;
    int in;
    int out;
    int done;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} buffer_t;

extern buffer_t buffer;

// Function prototypes
void *manager(void *arg);
void *worker(void *arg);
void init_buffer(buffer_t *buffer, int capacity);
void destroy_buffer(buffer_t *buffer);
void add_task(buffer_t *buffer, file_task_t task);
file_task_t get_task(buffer_t *buffer);

#endif // UTILS_H
