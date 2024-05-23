#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utils.h"

// Global variables
buffer_t buffer;

void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <buffer_size> <num_workers> <src_dir> <dst_dir>\n", prog_name);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        usage(argv[0]);
    }

    int buffer_size = atoi(argv[1]);
    int num_workers = atoi(argv[2]);
    const char *src_dir = argv[3];
    const char *dst_dir = argv[4];

    if (buffer_size <= 0 || num_workers <= 0) {
        usage(argv[0]);
    }

    // Initialize buffer
    init_buffer(&buffer, buffer_size);

    // Create manager thread
    // Define a new structure to hold the src_dir and dst_dir
    typedef struct {
        char *src_dir;
        char *dst_dir;
    } manager_args_t;

    // Create an instance of the structure and set the src_dir and dst_dir
    manager_args_t args;
    args.src_dir = src_dir;
    args.dst_dir = dst_dir;

    // Pass a pointer to the structure when creating the thread
    pthread_t manager_thread;
    if (pthread_create(&manager_thread, NULL, manager, (void *)&args) != 0) {
        perror("Failed to create manager thread");
        exit(EXIT_FAILURE);
    }

    // Create worker threads
    pthread_t worker_threads[num_workers];
    for (int i = 0; i < num_workers; i++) {
        if (pthread_create(&worker_threads[i], NULL, worker, (void *)dst_dir) != 0) {
            perror("Failed to create worker thread");
            exit(EXIT_FAILURE);
        }
    }

    // Wait for manager to finish
    pthread_join(manager_thread, NULL);

    // Wait for all workers to finish
    for (int i = 0; i < num_workers; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    // Clean up buffer
    destroy_buffer(&buffer);

    return 0;
}
