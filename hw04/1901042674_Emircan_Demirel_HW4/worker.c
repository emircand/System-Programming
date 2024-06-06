#include "utils.h"

int total_files_copied = 0;
int total_dirs_copied = 0;
int total_fifo_files_copied = 0;
ssize_t total_bytes_copied = 0;

void *worker(void *arg) {
    pthread_barrier_wait(&barrier);

    while (1) {
        pthread_mutex_lock(&buffer.mutex);

        while (buffer.count == 0 && !buffer.done) {
            pthread_cond_wait(&buffer.not_empty, &buffer.mutex);
        }

        if (buffer.count == 0 && buffer.done) {
            pthread_mutex_unlock(&buffer.mutex);
            break;
        }

        file_task_t task = get_task(&buffer);
        pthread_cond_signal(&buffer.not_full);
        pthread_mutex_unlock(&buffer.mutex);

        // Copy file content
        char buf[4096];
        ssize_t bytes;
        while ((bytes = read(task.src_fd, buf, sizeof(buf))) > 0) {
            if (write(task.dst_fd, buf, bytes) != bytes) {
                perror("write");
                break;
            }
            pthread_mutex_lock(&counter_mutex);
            total_bytes_copied += bytes;
            pthread_mutex_unlock(&counter_mutex);
        }

        close(task.src_fd);
        close(task.dst_fd);
    }

    pthread_barrier_wait(&barrier);

    pthread_exit(NULL);
}