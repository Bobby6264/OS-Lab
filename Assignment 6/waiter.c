#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/types.h>

#define TIME_NEEDED_TO_GET_ORDER 1
#define CLOSING_HOUR 240

// Global variables
int g_shmid = -1;
int g_semid = -1;
int *g_memory = NULL;

int g_min = 0;
int g_hr = 0;
char *g_period = "AM";

// Semaphore operations
void do_sem_op(int sid, int snum, int operation) {
    struct sembuf sb;
    sb.sem_op = operation;
    sb.sem_flg = 0;
    sb.sem_num = snum;
    sb.sem_num = snum;
    semop(sid, &sb, 1);
}

void sem_down(int sid, int snum) {
    do_sem_op(sid, snum, -1);
}

void sem_up(int sid, int snum) {
    do_sem_op(sid, snum, 1);
}

// Time management
void refresh_clock() {
    sem_down(g_semid, 0);
    int curr = g_memory[0];
    sem_up(g_semid, 0);

    g_hr = 11 + curr / 60;
    g_min = curr % 60;
    
    if (g_hr > 12) {
        g_hr -= 12;
    }
    
    if (g_hr >= 11 && g_hr < 12) {
        g_period = "AM";
    } else {
        g_period = "PM";
    }
}

void sleep_for(int wait_mins) {
    sem_down(g_semid, 0);
    int future = g_memory[0] + wait_mins;
    sem_up(g_semid, 0);

    usleep(wait_mins * 100000);

    sem_down(g_semid, 0);
    if (future < g_memory[0]) {
        printf("Warning: time update failed current: %5d <-> attempted: %5d\n", 
               g_memory[0], future);
    } else {
        g_memory[0] = future;
    }
    sem_up(g_semid, 0);
}

// Setup waiter workspace
int setup_waiter_workspace(int waiter_id) {
    int base_addr = 100 + waiter_id * 200;
    
    sem_down(g_semid, 0);
    g_memory[base_addr] = 0;     // Customer service slot
    g_memory[base_addr + 1] = 0; // Order count
    g_memory[base_addr + 2] = base_addr + 4; // Front pointer
    g_memory[base_addr + 3] = base_addr + 4; // Rear pointer
    sem_up(g_semid, 0);
    
    return base_addr;
}

// Waiter main function
void waiter_main(int waiter_id) {
    refresh_clock();
    printf("[%02d:%02d %s] Waiter %c is ready.\n", g_hr, g_min, g_period, waiter_id + 'U');
    fflush(stdout);

    int base_addr = setup_waiter_workspace(waiter_id);
    
    while (1) {
        sem_down(g_semid, 2 + waiter_id);
        sem_down(g_semid, 0);

        // Check if there's food to serve
        if (g_memory[base_addr] != 0) {
            int customer_id = g_memory[base_addr];
            g_memory[base_addr] = 0; // Reset food-ready flag
            sem_up(g_semid, 0);

            refresh_clock();
            printf("[%02d:%02d %s] Waiter %c: \t\tServing food to customer   %4d\n", 
                   g_hr, g_min, g_period, waiter_id + 'U', customer_id);
            fflush(stdout);

            // Notify customer that food is served
            sem_up(g_semid, 7 + customer_id - 1);

            sem_down(g_semid, 0);
            if ((g_memory[0] > CLOSING_HOUR) && g_memory[base_addr + 1] == 0) {
                sem_up(g_semid, 0);
                break;
            }
            sem_up(g_semid, 0);
        }
        // Check if there's an order to place
        else if (g_memory[base_addr + 1] != 0) {
            int front_ptr = g_memory[base_addr + 2];
            g_memory[base_addr + 1]--; // Decrement pending orders

            int customer_id = g_memory[front_ptr];
            int number_of_people = g_memory[front_ptr + 1];

            g_memory[base_addr + 2] = front_ptr + 2; // Update front pointer
            sem_up(g_semid, 0);

            // Taking order takes time
            sleep_for(TIME_NEEDED_TO_GET_ORDER);

            refresh_clock();
            printf("[%02d:%02d %s] Waiter %c: \tPlacing order for Customer %4d (count = %4d)\n", 
                   g_hr, g_min, g_period, waiter_id + 'U', customer_id, number_of_people);
            fflush(stdout);

            sem_down(g_semid, 0);
            // Add order to cook queue
            int rear_ptr = g_memory[1101];
            g_memory[rear_ptr] = waiter_id;
            g_memory[rear_ptr + 1] = customer_id;
            g_memory[rear_ptr + 2] = number_of_people;
            g_memory[1101] = rear_ptr + 3;
            sem_up(g_semid, 0);
            
            sem_up(g_semid, 1); // Signal cook
            sem_up(g_semid, 7 + customer_id - 1); // Signal customer that order is placed
        }
        else {
            if (g_memory[0] > CLOSING_HOUR) {
                sem_up(g_semid, 0);
                break;
            }
            sem_up(g_semid, 0);
        }
    }

    refresh_clock();
    printf("[%02d:%02d %s] Waiter %c leaving (no more customers to serve)\n", 
           g_hr, g_min, g_period, waiter_id + 'U');
    fflush(stdout);
}

// Initialize shared memory and semaphores
void init_resources() {
    key_t shm_key = ftok("makefile", 'B');
    g_shmid = shmget(shm_key, 2001 * sizeof(int), 0666);

    g_memory = (int *)shmat(g_shmid, NULL, 0);
    key_t sem_key = ftok("makefile", 'A');

    g_semid = semget(sem_key, 207, 0666);
}

int main() {
    pid_t pid;

    init_resources();
    
    for (int i = 0; i < 5; i++) {
        pid = fork();
        if (pid == 0) {
            waiter_main(i);
            shmdt(g_memory);
            exit(0);
        }
    }

    for (int i = 0; i < 5; i++) {
        wait(NULL);
    }

    shmdt(g_memory);
    printf("\nThe restaurant is closed.\n");
    fflush(stdout);
    
    return 0;
}
