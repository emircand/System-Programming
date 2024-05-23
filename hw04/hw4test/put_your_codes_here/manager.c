#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"


typedef struct {
    const char *src_dir;
    const char *dst_dir;
} manager_args_t;

void *manager(void *arg) {
    manager_args_t *args = (manager_args_t *)arg;
    const char *src_dir = args->src_dir;
    const char *dst_dir = args->dst_dir;
    struct dirent *entry;
    DIR *dp = opendir(src_dir);

    if (dp == NULL) {
        perror("opendir");
        pthread_exit(NULL);
    }

    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_type == DT_REG) {
            // Prepare file task
            file_task_t task;
            snprintf(task.src_filename, sizeof(task.src_filename), "%s/%s", src_dir, entry->d_name);
            snprintf(task.dst_filename, sizeof(task.dst_filename), "%s/%s", dst_dir, entry->d_name);

            task.src_fd = open(task.src_filename, O_RDONLY);
            if (task.src_fd == -1) {
                perror("open src");
                continue;
            }

            task.dst_fd = open(task.dst_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (task.dst_fd == -1) {
                perror("open dst");
                close(task.src_fd);
                continue;
            }

            // Add task to buffer
            add_task(&buffer, task);
        }
    }

    closedir(dp);

    // Signal that manager is done
    pthread_mutex_lock(&buffer.mutex);
    buffer.done = 1;
    pthread_cond_broadcast(&buffer.not_empty);
    pthread_mutex_unlock(&buffer.mutex);

    pthread_exit(NULL);
}
