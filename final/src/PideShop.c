#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

#define OVEN_CAPACITY 6
#define LOG_FILE "pideshop.log"
#define DELIVERY_CAPACITY 3
#define MAX_QUEUE 100

pthread_mutex_t oven_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t order_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t delivery_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t oven_cond = PTHREAD_COND_INITIALIZER;

typedef struct Order {
    char details[256];
    int cooked;
    int delivered;
    struct Order *next;
    int client_fd;
} Order;

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
    int location_x;
    int location_y;
    char order_details[256];
} ClientData;

Order *order_head = NULL;
Order *order_tail = NULL;

int oven_places = OVEN_CAPACITY;
int delivery_bag_count[MAX_QUEUE / DELIVERY_CAPACITY] = {0}; // Assuming enough delivery personnel

void log_activity(const char *message) {
    pthread_mutex_lock(&log_mutex);
    printf(">> %s\n", message);
    pthread_mutex_unlock(&log_mutex);
}

void handle_sigint(int sig) {
    log_activity("Server shutting down...");
    exit(0);
}

void enqueue_order(Order **head, Order **tail, Order *new_order) {
    if (*tail) {
        (*tail)->next = new_order;
    } else {
        *head = new_order;
    }
    *tail = new_order;
    new_order->next = NULL;
}

Order *dequeue_order(Order **head, Order **tail) {
    if (!*head) {
        return NULL;
    }
    Order *order = *head;
    *head = order->next;
    if (!*head) {
        *tail = NULL;
    }
    return order;
}

void *cook_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&order_mutex);
        while (!order_head || order_head->cooked) {
            pthread_cond_wait(&order_cond, &order_mutex);
        }
        Order *order = dequeue_order(&order_head, &order_tail);
        pthread_mutex_unlock(&order_mutex);

        if (order) {
            // Simulate the time to prepare the order (pseudo-inverse calculation)
            sleep(2);

            // Wait for an available oven place
            pthread_mutex_lock(&oven_mutex);
            while (oven_places == 0) {
                pthread_cond_wait(&oven_cond, &oven_mutex);
            }
            oven_places--;
            pthread_mutex_unlock(&oven_mutex);

            // Simulate the time to place the order in the oven
            sleep(1);

            pthread_mutex_lock(&oven_mutex);
            oven_places++;
            pthread_cond_signal(&oven_cond);
            pthread_mutex_unlock(&oven_mutex);

            pthread_mutex_lock(&order_mutex);
            order->cooked = 1;
            log_activity("Order cooked and placed in oven.");
            enqueue_order(&order_head, &order_tail, order); // Re-enqueue cooked order for delivery
            pthread_cond_signal(&delivery_cond);
            pthread_mutex_unlock(&order_mutex);
        }
    }
    return NULL;
}

void *delivery_thread(void *arg) {
    int delivery_person_id = *(int *)arg;
    while (1) {
        pthread_mutex_lock(&order_mutex);
        while (!order_head || !order_head->cooked || order_head->delivered) {
            pthread_cond_wait(&delivery_cond, &order_mutex);
        }
        Order *order = dequeue_order(&order_head, &order_tail);
        pthread_mutex_unlock(&order_mutex);

        if (order) {
            delivery_bag_count[delivery_person_id]++;
            if (delivery_bag_count[delivery_person_id] == DELIVERY_CAPACITY) {
                // Simulate delivery time
                sleep(5);

                pthread_mutex_lock(&order_mutex);
                order->delivered = 1;
                log_activity("Order delivered.");
                free(order);
                delivery_bag_count[delivery_person_id] = 0; // Reset the delivery bag

                // Check if all orders are delivered
                if (order_head == NULL) {
                    int client_sock = order->client_fd;
                    send(client_sock, "All orders delivered", 21, 0);
                }

                pthread_mutex_unlock(&order_mutex);
            } else {
                enqueue_order(&order_head, &order_tail, order);
                pthread_cond_signal(&delivery_cond); // Check for more orders
            }
        }
    }
    return NULL;
}

void *client_handler(void *arg) {
    ClientData *client_data = (ClientData *)arg;
    char buffer[1024];
    int bytes_read;

    while ((bytes_read = read(client_data->client_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("> %s\n", buffer);
        log_activity(buffer);

        // Extract location information from the buffer
        int location_x, location_y;
        sscanf(buffer, "Order from client %*d at location (%d, %d)", &location_x, &location_y);
        client_data->location_x = location_x;
        client_data->location_y = location_y;

        Order *new_order = (Order *)malloc(sizeof(Order));
        strcpy(new_order->details, buffer);
        new_order->client_fd = client_data->client_fd;
        new_order->cooked = 0;
        new_order->delivered = 0;
        new_order->next = NULL;

        pthread_mutex_lock(&order_mutex);
        enqueue_order(&order_head, &order_tail, new_order);
        pthread_cond_signal(&order_cond);
        pthread_mutex_unlock(&order_mutex);
    }

    pthread_mutex_lock(&order_mutex);
    while (order_head != NULL) {
        pthread_cond_wait(&delivery_cond, &order_mutex);
    }
    pthread_mutex_unlock(&order_mutex);

    close(client_data->client_fd);
    free(client_data);

    pthread_mutex_lock(&order_mutex);
    if (!order_head) {
        printf("> done serving client @ %s\n", inet_ntoa(client_data->client_addr.sin_addr));
    }
    pthread_mutex_unlock(&order_mutex);

    printf("> PideShop active waiting for connection ...\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <ip:port> <cook_thread_pool_size> <delivery_thread_pool_size> <delivery_speed>\n", argv[0]);
        exit(1);
    }

    char *ip_port = argv[1];
    char *ip = strtok(ip_port, ":");
    int port = atoi(strtok(NULL, ":"));

    int cook_thread_pool_size = atoi(argv[2]);
    int delivery_thread_pool_size = atoi(argv[3]);
    int delivery_speed = atoi(argv[4]);

    signal(SIGINT, handle_sigint);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip);
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_QUEUE) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("> PideShop %s %d %d %d\n", argv[1], cook_thread_pool_size, delivery_thread_pool_size, delivery_speed);
    printf("> PideShop active waiting for connection ...\n");

    log_activity("PideShop server started.");

    pthread_t cook_threads[cook_thread_pool_size];
    pthread_t delivery_threads[delivery_thread_pool_size];
    int delivery_ids[delivery_thread_pool_size];

    for (int i = 0; i < cook_thread_pool_size; i++) {
        pthread_create(&cook_threads[i], NULL, cook_thread, NULL);
    }

    for (int i = 0; i < delivery_thread_pool_size; i++) {
        delivery_ids[i] = i;
        pthread_create(&delivery_threads[i], NULL, delivery_thread, &delivery_ids[i]);
    }

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept failed");
            continue;
        }

        ClientData *client_data = (ClientData *)malloc(sizeof(ClientData));
        client_data->client_fd = new_socket;
        client_data->client_addr = address;

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, client_handler, (void *)client_data);
        pthread_detach(client_thread);
    }

    close(server_fd);
    return 0;
}
