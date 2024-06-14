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
    int number_of_orders;
    pid_t pid;
} OrderDetails;

pthread_mutex_t oven_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cook_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t delivery_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cook_queue_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t delivery_queue_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t oven_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t connection_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile int server_running = 1; // Global variable to control the server running state
// Global variable to indicate if a termination signal was received
volatile sig_atomic_t termination_signal_received = 0;
pthread_cond_t shutdown_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t shutdown_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t oven_apparatus_sem;
sem_t oven_opening_sem;

typedef struct Order {
    OrderDetails details;
    int cooked;
    int delivered;
    struct Order *next;
    int client_fd;
    int id;
    pid_t pid;
} Order;

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} ClientData;

typedef struct {
    int id;
    int orders_prepared;
} Cook;

Order *cook_queue_head = NULL;
Order *cook_queue_tail = NULL;
Order *delivery_queue_head = NULL;
Order *delivery_queue_tail = NULL;

int oven_places = OVEN_CAPACITY;
int delivery_bag_count[MAX_QUEUE / DELIVERY_CAPACITY] = {0}; // Assuming enough delivery personnel
int delivery_speed;
int *deliveries_per_person; // Array to track deliveries per delivery person
Cook *cooks; // Global pointer to store the array of cooks

int cook_thread_pool_size;
int delivery_thread_pool_size;

// Global variables
int total_orders_received = 0;
int total_orders_cooked = 0;
int total_orders_delivered = 0;

void print_order_statistics() {
    printf("Total orders received: %d\n", total_orders_received);
    printf("Total orders cooked: %d\n", total_orders_cooked);
    printf("Total orders delivered: %d\n", total_orders_delivered);
}

// Function to log activity
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
        fprintf(log_file, "[%s]\t<< Order : %d >>\t%s\n", time_str, order_id, message);
    else
        fprintf(log_file, "[%s]\t%s\n", time_str, message);
    pthread_mutex_unlock(&log_mutex);
    fclose(log_file);
}

// Signal handler for SIGINT
void handle_sigint(int sig) {
    log_activity("SIGINT received. Shutting down server...", -1);
    print_order_statistics();
    termination_signal_received = 1;

    // Wake up all threads to ensure they can check the termination signal
    pthread_mutex_lock(&shutdown_mutex);
    pthread_cond_broadcast(&shutdown_cond);
    pthread_mutex_unlock(&shutdown_mutex);
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
    while (server_running) {
        pthread_mutex_lock(&cook_queue_mutex);
        while (!cook_queue_head && server_running) {
            pthread_cond_wait(&cook_queue_cond, &cook_queue_mutex);
        }
        if (!server_running) {
            pthread_mutex_unlock(&cook_queue_mutex);
            break;
        }
        Order *order = dequeue_order(&cook_queue_head, &cook_queue_tail);
        pthread_mutex_unlock(&cook_queue_mutex);

        if (order) {
            // Simulate the time to prepare the order (pseudo-inverse calculation)
            sleep(2);

            total_orders_cooked++;
            log_activity("Prepared by the cook", order->id);
            // Wait for an available oven apparatus and opening to place the meal in the oven
            sem_wait(&oven_apparatus_sem);
            sem_wait(&oven_opening_sem);

            pthread_mutex_lock(&oven_mutex);
            while (oven_places == 0) {
                pthread_cond_wait(&oven_cond, &oven_mutex);
            }
            oven_places--;
            log_activity("Placed in the oven", order->id);
            pthread_mutex_unlock(&oven_mutex);

            // Release the oven opening after placing the meal
            sem_post(&oven_opening_sem);

            // Simulate cooking time in the oven
            sleep(3);

            // Wait for an available oven opening to remove the meal
            sem_wait(&oven_opening_sem);

            pthread_mutex_lock(&oven_mutex);
            oven_places++;
            pthread_cond_signal(&oven_cond);
            pthread_mutex_unlock(&oven_mutex);

            // Release the oven apparatus after removing the meal
            sem_post(&oven_apparatus_sem);
            // Release the oven opening after removing the meal
            sem_post(&oven_opening_sem);

            pthread_mutex_lock(&delivery_queue_mutex);
            order->cooked = 1;
            cook->orders_prepared++;
            log_activity("Cooked", order->id);
            enqueue_order(&delivery_queue_head, &delivery_queue_tail, order); // Enqueue cooked order for delivery
            pthread_cond_signal(&delivery_queue_cond);
            pthread_mutex_unlock(&delivery_queue_mutex);
        }
    }
    return NULL;
}

void *delivery_thread(void *arg) {
    int delivery_person_id = *(int *)arg;
    while (server_running) {
        pthread_mutex_lock(&delivery_queue_mutex);
        while (!delivery_queue_head && server_running) {
            pthread_cond_wait(&delivery_queue_cond, &delivery_queue_mutex);
        }
        if (!server_running) {
            pthread_mutex_unlock(&delivery_queue_mutex);
            break;
        }
        Order *order = dequeue_order(&delivery_queue_head, &delivery_queue_tail);
        log_activity("Handed out for delivery", order->id);
        pthread_mutex_unlock(&delivery_queue_mutex);

        if (order) {
            deliveries_per_person[delivery_person_id]++;
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
            log_activity("Delivered... ", order->id);
            free(order);
            delivery_bag_count[delivery_person_id] = 0; // Reset the delivery bag

            // Check if all orders are delivered
            if (delivery_queue_head == NULL && cook_queue_head == NULL) {
                int client_sock = order->client_fd;
                send(client_sock, "All orders served. Closing connection.", 38, 0);
                close(client_sock);
            }

            pthread_mutex_unlock(&order_mutex);
        }
    }
    return NULL;
}

// Function to promote the most efficient delivery person
void promote_best_delivery_person(int delivery_thread_pool_size) {
    int max_deliveries = 0;
    int best_delivery_person = -1;
    for (int i = 0; i < delivery_thread_pool_size; i++) {
        if (deliveries_per_person[i] > max_deliveries) {
            max_deliveries = deliveries_per_person[i];
            best_delivery_person = i;
        }
    }
    if (best_delivery_person != -1) {
        printf("Promoting Moto %d with %d deliveries.\n", best_delivery_person, max_deliveries);
    }
}

// Function to promote the most efficient cook
void promote_best_cook(int cook_thread_pool_size) {
    int max_orders_prepared = 0;
    int best_cook = -1;
    for (int i = 0; i < cook_thread_pool_size; i++) {
        if (cooks[i].orders_prepared > max_orders_prepared) {
            max_orders_prepared = cooks[i].orders_prepared;
            best_cook = i;
        }
    }
    if (best_cook != -1) {
        printf("Promoting Cook %d with %d orders prepared.\n", best_cook, max_orders_prepared);
    }
}

// Client handler function
void *client_handler(void *arg) {
    ClientData *client_data = (ClientData *)arg;
    OrderDetails order_details;
    int bytes_read;

    while ((bytes_read = read(client_data->client_fd, &order_details, sizeof(OrderDetails))) > 0) {
        if (strcmp((char *)&order_details, "TERMINATE") == 0) {
            // Log the termination request and close the connection
            log_activity("Received TERMINATE message from client", -1);
            close(client_data->client_fd);
            free(client_data);
            return NULL;
        }

        total_orders_received++;

        Order *new_order = (Order *)malloc(sizeof(Order));
        new_order->details = order_details;
        new_order->client_fd = client_data->client_fd;
        new_order->id = order_details.client_id;
        new_order->cooked = 0;
        new_order->delivered = 0;
        new_order->next = NULL;
        new_order->pid = order_details.pid;

        pthread_mutex_lock(&cook_queue_mutex);
        enqueue_order(&cook_queue_head, &cook_queue_tail, new_order);
        pthread_cond_signal(&cook_queue_cond);
        pthread_mutex_unlock(&cook_queue_mutex);
    }

    pthread_mutex_lock(&delivery_queue_mutex);
    while (delivery_queue_head != NULL) {
        pthread_cond_wait(&delivery_queue_cond, &delivery_queue_mutex);
    }
    pthread_mutex_unlock(&delivery_queue_mutex);

    close(client_data->client_fd);
    free(client_data);

    pthread_mutex_lock(&cook_queue_mutex);
    if (!cook_queue_head) {
        printf("> done serving client @%d\n", order_details.pid);
        // At the end of the day, promote the best delivery person
        promote_best_delivery_person(delivery_thread_pool_size);
        // At the end of the day, promote the best cook
        promote_best_cook(cook_thread_pool_size);
    }
    pthread_mutex_unlock(&cook_queue_mutex);

    printf("> PideShop active waiting for connection ...\n");

    // Unlock the connection mutex to allow new connections
    pthread_mutex_unlock(&connection_mutex);

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

    cook_thread_pool_size = atoi(argv[2]);
    delivery_thread_pool_size = atoi(argv[3]);
    delivery_speed = atoi(argv[4]);

    deliveries_per_person = (int *)malloc(delivery_thread_pool_size * sizeof(int));
    memset(deliveries_per_person, 0, delivery_thread_pool_size * sizeof(int));

    cooks = (Cook *)malloc(cook_thread_pool_size * sizeof(Cook)); // Allocate memory for cooks
    if (cooks == NULL) {
        perror("malloc");
        exit(1);
    }

    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigint);

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

    for (int i = 0; i < cook_thread_pool_size; i++) {
        cooks[i].id = i;
        cooks[i].orders_prepared = 0;
        pthread_create(&cook_threads[i], NULL, cook_thread, &cooks[i]);
    }

    for (int i = 0; i < delivery_thread_pool_size; i++) {
        delivery_ids[i] = i;
        pthread_create(&delivery_threads[i], NULL, delivery_thread, &delivery_ids[i]);
    }

    while (server_running) {
        // Lock the connection mutex to ensure only one client connection is processed at a time
        pthread_mutex_lock(&connection_mutex);

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            if (termination_signal_received) {
                pthread_mutex_unlock(&connection_mutex);
                break;
            }
            perror("accept failed");
            pthread_mutex_unlock(&connection_mutex);
            continue;
        }

        ClientData *client_data = (ClientData *)malloc(sizeof(ClientData));
        client_data->client_fd = new_socket;
        client_data->client_addr = address;

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, client_handler, (void *)client_data);
        pthread_detach(client_thread);
    }

    // Wait for all threads to finish
    for (int i = 0; i < cook_thread_pool_size; i++) {
        pthread_join(cook_threads[i], NULL);
    }
    for (int i = 0; i < delivery_thread_pool_size; i++) {
        pthread_join(delivery_threads[i], NULL);
    }

    close(server_fd);
    free(deliveries_per_person);
    free(cooks); // Free the allocated memory for cooks
    return 0;
}
