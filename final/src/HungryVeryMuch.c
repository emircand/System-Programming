#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>

typedef struct {
    int client_id;
    int location_x;
    int location_y;
    int town_size_x;
    int town_size_y;
    int number_of_orders;
    int is_terminated;
    pid_t pid;
} OrderDetails;

pthread_mutex_t delivery_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t delivery_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t order_send_mutex = PTHREAD_MUTEX_INITIALIZER;
int all_orders_delivered = 0;
int sock = 0;
volatile sig_atomic_t stop_requested = 0;

void handle_sigint(int sig) {
    printf(">^C signal .. cancelling orders.. editing log..\n");
    stop_requested = 1;

    OrderDetails termination_message;
    termination_message.is_terminated = 1;
    termination_message.pid = getpid();
    send(sock, &termination_message, sizeof(OrderDetails), 0);

    pthread_mutex_lock(&order_send_mutex);
    all_orders_delivered = 1;
    pthread_cond_signal(&delivery_cond);
    pthread_mutex_unlock(&order_send_mutex);

    exit(0);
}

void *wait_for_delivery(void *sock_ptr) {
    int sock = *(int *)sock_ptr;
    char buffer[1024];
    int bytes_read;

    while ((bytes_read = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        if (strcmp(buffer, "All orders served. Closing connection.") == 0) {
            pthread_mutex_lock(&delivery_mutex);
            all_orders_delivered = 1;
            pthread_cond_signal(&delivery_cond);
            pthread_mutex_unlock(&delivery_mutex);
            break;
        }
    }

    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <ip:port> <number_of_clients> <town_size_x> <town_size_y>\n", argv[0]);
        exit(1);
    }

    char *ip_port = argv[1];
    char *ip = strtok(ip_port, ":");
    int port = atoi(strtok(NULL, ":"));

    int number_of_clients = atoi(argv[2]);
    int town_size_x = atoi(argv[3]);
    int town_size_y = atoi(argv[4]);

    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigint);

    sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        exit(1);
    }

    pthread_t delivery_thread;
    pthread_create(&delivery_thread, NULL, wait_for_delivery, &sock);
    pthread_detach(delivery_thread);

    printf("> PID: %d\n", getpid());
    printf("> Sending %d orders to %s:%d\n", number_of_clients, ip, port);

    for (int i = 0; i < number_of_clients && !stop_requested; i++) {
        OrderDetails order;
        order.client_id = i;
        order.location_x = rand() % town_size_x;
        order.location_y = rand() % town_size_y;
        order.town_size_x = town_size_x;
        order.town_size_y = town_size_y;
        order.number_of_orders = number_of_clients;
        order.pid = getpid();
        send(sock, &order, sizeof(OrderDetails), 0);
    }

    pthread_mutex_lock(&delivery_mutex);
    while (!all_orders_delivered) {
        pthread_cond_wait(&delivery_cond, &delivery_mutex);
    }
    pthread_mutex_unlock(&delivery_mutex);

    printf("> All customers served\n> log file written ..\n");

    return 0;
}
