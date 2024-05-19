#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>

#define TOTAL_AUTOMOBILE_SPACES 8
#define TOTAL_PICKUP_SPACES 4
#define TOTAL_THREADS 100
pthread_t threads[TOTAL_THREADS];

// Semaphores for synchronization
sem_t newPickup, inChargeforPickup;
sem_t newAutomobile, inChargeforAutomobile;

// Shared variables
int mFree_automobile = TOTAL_AUTOMOBILE_SPACES;
int mFree_pickup = TOTAL_PICKUP_SPACES;
int running = 1;
int active_threads = 0;

enum VehicleType {
    CAR = 1,
    PICKUP = 2
};

enum VehicleType generate_vehicle_type() {
    return rand() % 2 == 0 ? 2 : 1; // 1 for car, 2 for pickup
}

// Thread function for car owners

void* carOwner(void* arg) {
    enum VehicleType vehicleType = generate_vehicle_type();
    printf(">> Car owner arrives with a vehicle type: ");
    switch (vehicleType) {
        case CAR:
            printf("Automobile\n");
            printf(">> Checking space for an automobile...\n");
            if (mFree_automobile > 0) {
                sem_wait(&newAutomobile);
                mFree_automobile--;
                printf("++ Space confirmed. Automobile parked, remaining spaces: %d / %d\n", mFree_automobile, TOTAL_AUTOMOBILE_SPACES);
                sleep(10);
                sem_post(&inChargeforAutomobile);
            } else {
                printf("<< No space available for an automobile. Exiting.\n");
            }
            break;
        case PICKUP:
            printf("Pickup\n");
            printf(">> Checking space for a pickup...\n");
            if (mFree_pickup > 0) {
                sem_wait(&newPickup);
                mFree_pickup--;
                printf("++ Space confirmed. Pickup parked, remaining spaces: %d / %d \n", mFree_pickup, TOTAL_PICKUP_SPACES);
                sleep(10);
                sem_post(&inChargeforPickup);
            } else {
                printf("<< No space available for a pickup. Exiting.\n");
            }
            break;
    }
    return NULL;
}

void* carAttendant(void* arg) {
    while (running) {
        int attendantType = *(int*)arg;
        if (attendantType == 1) { // Attendant for automobiles
            sem_wait(&inChargeforAutomobile);
            mFree_automobile++;
            printf("-- Automobile retrieved, available automobile spaces : %d / %d\n", mFree_automobile, TOTAL_AUTOMOBILE_SPACES);
            sleep(1);
            sem_post(&newAutomobile);
        } else { // Attendant for pickups
            sem_wait(&inChargeforPickup);
            mFree_pickup++;
            printf("-- Pickup retrieved, available pickup spaces : %d / %d\n", mFree_pickup, TOTAL_PICKUP_SPACES);
            sleep(1);
            sem_post(&newPickup);
        }
    }
    return NULL;
}

void my_cleaner() {
    // Thread cleanup
    for (int i = 0; i < active_threads; i++) {
        pthread_cancel(threads[i]);  // Send cancellation request to the thread
        pthread_join(threads[i], NULL);  // Wait for the thread to terminate
    }

    // Semaphore destruction
    sem_destroy(&newPickup);  // Destroy semaphore for new pickup signal
    sem_destroy(&inChargeforPickup);  // Destroy semaphore for pickup in charge signal
    sem_destroy(&newAutomobile);  // Destroy semaphore for new automobile signal
    sem_destroy(&inChargeforAutomobile);  // Destroy semaphore for automobile in charge signal
}

void handle_sigint(int sig) {
    printf("Handling SIGINT.. Exiting..\n");

    // Destroy semaphores and cancel threads
    my_cleaner();
    
    exit(0);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
    srand(time(NULL));
    int attendantAuto = 1, attendantPickup = 2;

    // Initialize semaphores
    sem_init(&newPickup, 0, TOTAL_PICKUP_SPACES);
    sem_init(&inChargeforPickup, 0, 0);
    sem_init(&newAutomobile, 0, TOTAL_AUTOMOBILE_SPACES);
    sem_init(&inChargeforAutomobile, 0, 0);

    // Create threads for 2 valets
    pthread_create(&threads[0], NULL, carAttendant, &attendantAuto);
    pthread_create(&threads[1], NULL, carAttendant, &attendantPickup);
    active_threads += 2;

    // Create threads for 8 car owners
    for (int i = 2; i < TOTAL_THREADS; i++) {
        pthread_create(&threads[i], NULL, carOwner, NULL);
        active_threads++;
        sleep(1);
        if(i == TOTAL_THREADS - 1) {
            running = 0;
            my_cleaner();
            exit(EXIT_SUCCESS);
        }
    }

    /* Possibly not needed */
    // Wait for all threads to complete
    for (int i = 0; i < TOTAL_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

