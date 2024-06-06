#include "utils.h"

void init_buffer(buffer_t *buffer, int capacity) {
    buffer->tasks = malloc(capacity * sizeof(file_task_t));
    buffer->capacity = capacity;
    buffer->count = 0;
    buffer->in = 0;
    buffer->out = 0;
    buffer->done = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->not_empty, NULL);
    pthread_cond_init(&buffer->not_full, NULL);
}

void destroy_buffer(buffer_t *buffer) {
    free(buffer->tasks);
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->not_empty);
    pthread_cond_destroy(&buffer->not_full);
    pthread_mutex_destroy(&counter_mutex);
}

void add_task(buffer_t *buffer, file_task_t task) {
    pthread_mutex_lock(&buffer->mutex);

    while (buffer->count == buffer->capacity) {
        pthread_cond_wait(&buffer->not_full, &buffer->mutex);
    }

    buffer->tasks[buffer->in] = task;
    buffer->in = (buffer->in + 1) % buffer->capacity;
    buffer->count++;
    total_files_copied++;

    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
}

file_task_t get_task(buffer_t *buffer) {
    file_task_t task = buffer->tasks[buffer->out];
    buffer->out = (buffer->out + 1) % buffer->capacity;
    buffer->count--;
    return task;
}
