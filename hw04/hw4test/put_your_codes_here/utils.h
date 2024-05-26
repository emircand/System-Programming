#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>
#include <sys/types.h>

#define PATH_MAX 4096

typedef struct {
    char src_filename[PATH_MAX];
    char dst_filename[PATH_MAX];
    int src_fd;
    int dst_fd;
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
extern int total_files_copied;
extern int total_dirs_copied;
extern int total_fifo_files_copied;
extern ssize_t total_bytes_copied;

extern pthread_barrier_t barrier;
extern pthread_mutex_t counter_mutex;

void init_buffer(buffer_t *buffer, int capacity);
void destroy_buffer(buffer_t *buffer);
void add_task(buffer_t *buffer, file_task_t task);
file_task_t get_task(buffer_t *buffer);
void *manager(void *arg);
void *worker(void *arg);

#endif // UTILS_H
