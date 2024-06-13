#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <semaphore.h>

#define OVEN_CAPACITY 6
#define LOG_FILE "pideshop.log"
#define DELIVERY_CAPACITY 3
#define MAX_QUEUE 100
#define OVEN_APPARATUS 3
#define OVEN_OPENINGS 2

typedef struct {
    int client_id;
    int location_x;
    int location_y;
    int town_size_x;
    int town_size_y;
} OrderDetails;

pthread_mutex_t oven_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t order_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t delivery_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t oven_cond = PTHREAD_COND_INITIALIZER;

sem_t oven_apparatus_sem;
sem_t oven_opening_sem;

typedef struct Order {
    OrderDetails details;
    int cooked;
    int delivered;
    struct Order *next;
    int client_fd;
    int id;
} Order;

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} ClientData;

typedef struct {
    int id;
    int orders_prepared;
} Cook;

Order *order_head = NULL;
Order *order_tail = NULL;

int oven_places = OVEN_CAPACITY;
int delivery_bag_count[MAX_QUEUE / DELIVERY_CAPACITY] = {0}; // Assuming enough delivery personnel
int delivery_speed;

// Global variables
int total_orders_received = 0;
int total_orders_cooked = 0;
int total_orders_delivered = 0;

void print_order_statistics() {
    printf("Total orders received: %d\n", total_orders_received);
    printf("Total orders cooked: %d\n", total_orders_cooked);
    printf("Total orders delivered: %d\n", total_orders_delivered);
}

void log_activity(const char *message, int order_id) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        perror("fopen");
        exit(1);
    }
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Remove newline character from the end
    pthread_mutex_lock(&log_mutex);
    if(order_id != -1)
        fprintf(log_file, "[%s] %s %d\n", time_str, message, order_id);
    else
        fprintf(log_file, "[%s] %s\n", time_str, message);
    pthread_mutex_unlock(&log_mutex);
    fclose(log_file);
}

void handle_sigint(int sig) {
    log_activity("Server shutting down...", -1);
    print_order_statistics();
    sem_destroy(&oven_apparatus_sem);
    sem_destroy(&oven_opening_sem);
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

// Function to calculate the square root using the iterative method
double sqrt_iterative(double number) {
    double guess = number / 2.0;
    double epsilon = 0.01; // Precision
    while ((guess * guess - number) > epsilon || (number - guess * guess) > epsilon) {
        guess = (guess + number / guess) / 2.0;
    }
    return guess;
}

// Function to calculate the hypotenuse
double calculate_hypotenuse(int x, int y) {
    return sqrt_iterative(x * x + y * y);
}

void *cook_thread(void *arg) {
    Cook *cook = (Cook *)arg;
    while (1) {
        // Lock the order mutex and wait for an order
        if (pthread_mutex_lock(&order_mutex) != 0) {
            perror("pthread_mutex_lock");
            continue;
        }
        while (!order_head || order_head->cooked) {
            if (pthread_cond_wait(&order_cond, &order_mutex) != 0) {
                perror("pthread_cond_wait");
                pthread_mutex_unlock(&order_mutex);
                continue;
            }
        }
        Order *order = dequeue_order(&order_head, &order_tail);
        if (pthread_mutex_unlock(&order_mutex) != 0) {
            perror("pthread_mutex_unlock");
            continue;
        }

        if (order) {
            // Simulate the time to prepare the order (pseudo-inverse calculation)
            sleep(2);

            total_orders_cooked++;

            // Wait for an available oven apparatus and opening to place the meal in the oven
            if (sem_wait(&oven_apparatus_sem) != 0) {
                perror("sem_wait (oven_apparatus_sem)");
                continue;
            }
            if (sem_wait(&oven_opening_sem) != 0) {
                perror("sem_wait (oven_opening_sem)");
                sem_post(&oven_apparatus_sem);
                continue;
            }

            if (pthread_mutex_lock(&oven_mutex) != 0) {
                perror("pthread_mutex_lock (oven_mutex)");
                sem_post(&oven_opening_sem);
                sem_post(&oven_apparatus_sem);
                continue;
            }
            while (oven_places == 0) {
                if (pthread_cond_wait(&oven_cond, &oven_mutex) != 0) {
                    perror("pthread_cond_wait (oven_cond)");
                    pthread_mutex_unlock(&oven_mutex);
                    sem_post(&oven_opening_sem);
                    sem_post(&oven_apparatus_sem);
                    continue;
                }
            }
            oven_places--;
            if (pthread_mutex_unlock(&oven_mutex) != 0) {
                perror("pthread_mutex_unlock (oven_mutex)");
                sem_post(&oven_opening_sem);
                sem_post(&oven_apparatus_sem);
                continue;
            }

            // Simulate the time to place the order in the oven
            sleep(1);

            // Release the oven opening after placing the meal
            if (sem_post(&oven_opening_sem) != 0) {
                perror("sem_post (oven_opening_sem)");
                sem_post(&oven_apparatus_sem);
                continue;
            }

            // Simulate cooking time in the oven
            sleep(3);

            // Wait for an available oven opening to remove the meal
            if (sem_wait(&oven_opening_sem) != 0) {
                perror("sem_wait (oven_opening_sem)");
                sem_post(&oven_apparatus_sem);
                continue;
            }

            if (pthread_mutex_lock(&oven_mutex) != 0) {
                perror("pthread_mutex_lock (oven_mutex)");
                sem_post(&oven_opening_sem);
                sem_post(&oven_apparatus_sem);
                continue;
            }
            oven_places++;
            if (pthread_cond_signal(&oven_cond) != 0) {
                perror("pthread_cond_signal (oven_cond)");
            }
            if (pthread_mutex_unlock(&oven_mutex) != 0) {
                perror("pthread_mutex_unlock (oven_mutex)");
                sem_post(&oven_opening_sem);
                sem_post(&oven_apparatus_sem);
                continue;
            }

            // Release the oven apparatus after removing the meal
            if (sem_post(&oven_apparatus_sem) != 0) {
                perror("sem_post (oven_apparatus_sem)");
                sem_post(&oven_opening_sem);
                continue;
            }
            // Release the oven opening after removing the meal
            if (sem_post(&oven_opening_sem) != 0) {
                perror("sem_post (oven_opening_sem)");
                continue;
            }

            if (pthread_mutex_lock(&order_mutex) != 0) {
                perror("pthread_mutex_lock (order_mutex)");
                continue;
            }
            order->cooked = 1;
            cook->orders_prepared++;
            log_activity("Order cooked and ready for delivery.", order->id);
            enqueue_order(&order_head, &order_tail, order); // Re-enqueue cooked order for delivery
            if (pthread_cond_signal(&delivery_cond) != 0) {
                perror("pthread_cond_signal (delivery_cond)");
            }
            if (pthread_mutex_unlock(&order_mutex) != 0) {
                perror("pthread_mutex_unlock (order_mutex)");
                continue;
            }
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
                // Calculate delivery time based on distance
                int town_width = order->details.town_size_x;
                int town_height = order->details.town_size_y;
                int shop_x = town_width / 2;
                int shop_y = town_height / 2;
                double distance = calculate_hypotenuse(shop_x - order->details.location_x, shop_y - order->details.location_y);
                sleep(distance / delivery_speed);

                total_orders_delivered++;

                pthread_mutex_lock(&order_mutex);
                order->delivered = 1;
                log_activity("Order delivered.", order->id);
                free(order);
                delivery_bag_count[delivery_person_id] = 0; // Reset the delivery bag

                // Check if all orders are delivered
                if (order_head == NULL) {
                    int client_sock = order->client_fd;
                    send(client_sock, "All orders served. Closing connection.", 38, 0);
                    close(client_sock);
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
    OrderDetails order_details;
    int bytes_read;

    while ((bytes_read = read(client_data->client_fd, &order_details, sizeof(OrderDetails))) > 0) {
        total_orders_received++;

        Order *new_order = (Order *)malloc(sizeof(Order));
        new_order->details = order_details;
        new_order->client_fd = client_data->client_fd;
        new_order->id = order_details.client_id;
        new_order->cooked = 0;
        new_order->delivered = 0;
        new_order->next = NULL;
        log_activity("Received new order.", new_order->id);

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
    delivery_speed = atoi(argv[4]);

    signal(SIGINT, handle_sigint);

    // Initialize semaphores
    sem_init(&oven_apparatus_sem, 0, OVEN_APPARATUS);
    sem_init(&oven_opening_sem, 0, OVEN_OPENINGS);

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

    log_activity("PideShop server started.", -1);

    pthread_t cook_threads[cook_thread_pool_size];
    pthread_t delivery_threads[delivery_thread_pool_size];
    int delivery_ids[delivery_thread_pool_size];
    Cook cooks[cook_thread_pool_size];

    for (int i = 0; i < cook_thread_pool_size; i++) {
        cooks[i].id = i;
        cooks[i].orders_prepared = 0;
        pthread_create(&cook_threads[i], NULL, cook_thread, &cooks[i]);
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
