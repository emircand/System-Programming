#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "utils.h"


void *worker(void *arg) {
    char *dst_dir = (char *)arg;
    (void)dst_dir;

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
        char buf[1024];
        ssize_t bytes;
        while ((bytes = read(task.src_fd, buf, sizeof(buf))) > 0) {
            if (write(task.dst_fd, buf, bytes) != bytes) {
                perror("write");
                break;
            }
        }

        close(task.src_fd);
        close(task.dst_fd);

        fprintf(stdout, "Copied %s to %s\n", task.src_filename, task.dst_filename);
    }

    pthread_exit(NULL);
}
