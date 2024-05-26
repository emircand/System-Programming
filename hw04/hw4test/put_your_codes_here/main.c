#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "utils.h"
#include <signal.h>

// Global variables
buffer_t buffer;

pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier;

int buffer_size;
int num_workers;

// Function prototypes
void usage(const char *prog_name);
void init_buffer(buffer_t *buffer, int capacity);
void destroy_buffer(buffer_t *buffer);
void collect_statistics();

typedef struct {
    const char *src_dir;
    const char *dst_dir;
} manager_args_t;

void sigint_handler(int signo) {
    destroy_buffer(&buffer);
    printf("Handling SIGINT... Exiting... \n");
    exit(EXIT_SUCCESS);
}


void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <buffer_size> <num_workers> <src_dir> <dst_dir>\n", prog_name);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    if (argc != 5) {
        usage(argv[0]);
    }

    buffer_size = atoi(argv[1]);
    num_workers = atoi(argv[2]);
    const char *src_dir = argv[3];
    const char *dst_dir = argv[4];

    if (buffer_size <= 0 || num_workers <= 0) {
        usage(argv[0]);
    }

    // Initialize buffer
    init_buffer(&buffer, buffer_size);

    // Measure execution time
    clock_t start_time = clock();

    // Create manager thread
    manager_args_t args = {src_dir, dst_dir};

    if (pthread_barrier_init(&barrier, NULL, num_workers+1) != 0) {
        perror("Failed to initialize barrier");
        exit(EXIT_FAILURE);
    }

    pthread_t manager_thread;
    if (pthread_create(&manager_thread, NULL, manager, (void *)&args) != 0) {
        perror("Failed to create manager thread");
        exit(EXIT_FAILURE);
    }

    // Create worker threads
    pthread_t worker_threads[num_workers];
    for (int i = 0; i < num_workers; i++) {
        if (pthread_create(&worker_threads[i], NULL, worker, NULL) != 0) {
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

    // Measure end time
    clock_t end_time = clock();
    double execution_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Total execution time: %.2f seconds\n", execution_time);

    // Collect and print statistics
    collect_statistics();

    pthread_barrier_destroy(&barrier);

    // Clean up buffer
    destroy_buffer(&buffer);

    return 0;
}

void collect_statistics() {
    extern int total_files_copied;
    extern int total_dirs_copied;
    extern int total_fifo_files_copied;
    extern ssize_t total_bytes_copied;

    printf("\n---------------STATISTICS--------------------\n");
    printf("Consumers: %d - Buffer Size: %d\n", num_workers, buffer_size);
    printf("Total files copied: %d\n", total_files_copied);
    printf("Total directories copied: %d\n", total_dirs_copied);
    printf("Total FIFOs copied: %d\n", total_fifo_files_copied);
    printf("Total bytes copied: %zd\n", total_bytes_copied);
}
