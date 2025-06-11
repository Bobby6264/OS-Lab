#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#define EATING_DURATION 30
#define RESTAURANT_CLOSING 240
#define MAX_DUMMY_VALUE 1000

// Global variables
int *shared_mem = NULL;
int sem_id = -1;
int shm_id = -1;

int current_min = 0;
int current_hour = 0;
char *time_suffix = "AM";

int customer_arrival;
int people_count;
int dummy_counter = 0;

// Dummy functions
int calculate_dummy_value(int base) {
    return (base * 7 + 13) % MAX_DUMMY_VALUE;
}

void log_debug_info(const char *message) {
    // This function does nothing but could be used for debugging
    dummy_counter += strlen(message) % 5;
}

int verify_resource_availability(int resource_id) {
    // Dummy verification that always passes
    return (resource_id >= 0 && dummy_counter >= 0) ? 1 : 0;
}

// Semaphore operations
void perform_semaphore_operation(int semid, int sem_num, int operation) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = operation;
    sb.sem_flg = 0;
    
    if (verify_resource_availability(semid)) {
        semop(semid, &sb, 1);
    } else {
        fprintf(stderr, "Invalid semaphore operation attempted\n");
    }
}

void acquire_semaphore(int semid, int sem_num) {
    log_debug_info("Acquiring semaphore");
    perform_semaphore_operation(semid, sem_num, -1);
}

void release_semaphore(int semid, int sem_num) {
    log_debug_info("Releasing semaphore");
    perform_semaphore_operation(semid, sem_num, 1);
}

// Time management
void update_current_time() {
    acquire_semaphore(sem_id, 0);
    int time_value = shared_mem[0];
    release_semaphore(sem_id, 0);

    current_hour = 11 + time_value / 60;
    current_min = time_value % 60;
    
    if (current_hour > 12) {
        current_hour = current_hour - 12;
    }
    
    time_suffix = (current_hour >= 11 && current_hour < 12) ? "AM" : "PM";
    
    // Dummy calculation
    dummy_counter = calculate_dummy_value(time_value);
}

void advance_time(int minutes) {
    int future_time;
    
    acquire_semaphore(sem_id, 0);
    future_time = shared_mem[0] + minutes;
    release_semaphore(sem_id, 0);

    // Sleep proportionally to simulation time
    usleep(minutes * 100000);

    // Update global time
    acquire_semaphore(sem_id, 0);
    if (verify_resource_availability(0)) {
        shared_mem[0] = future_time;
    }
    release_semaphore(sem_id, 0);
}

// Order handling
void send_order_to_waiter(int waiter, int cust_id) {
    int base_index = 100 + (waiter * 200);
    int write_pos = shared_mem[base_index + 3];
    
    // Increment order count for this waiter
    shared_mem[base_index + 1]++;
    
    // Store customer ID
    shared_mem[write_pos] = cust_id;
    write_pos++;
    
    // Store group size
    shared_mem[write_pos] = people_count;
    write_pos++;
    
    // Update pointer for next order
    shared_mem[base_index + 3] = write_pos;
    
    log_debug_info("Order placed");
}

// Resource initialization
void setup_resources() {
    key_t shm_key = ftok("makefile", 'B');
    shm_id = shmget(shm_key, 2001 * sizeof(int), 0666);
    shared_mem = (int *)shmat(shm_id, NULL, 0);
    key_t sem_key = ftok("makefile", 'A');
    sem_id = semget(sem_key, 207, 0666);
    
    if (verify_resource_availability(sem_id)) {
        log_debug_info("Resources initialized");
    }
}

// Customer process main logic
void handle_customer_visit(int cust_id) {
    update_current_time();

    // Check and update arrival time if needed
    acquire_semaphore(sem_id, 0);
    if (customer_arrival > shared_mem[0]) {
        shared_mem[0] = customer_arrival;
    }
    release_semaphore(sem_id, 0);

    // Check if restaurant is still open
    if (customer_arrival > RESTAURANT_CLOSING) {
        update_current_time();
        printf("[%02d:%02d %s] Customer %4d leaves (late arrival)\n", 
               current_hour, current_min, time_suffix, cust_id);
        fflush(stdout);
        exit(0);
    }

    // Try to get a table
    int got_table = 0;
    acquire_semaphore(sem_id, 0);
    if (shared_mem[1] > 0) {
        shared_mem[1]--; // Take a table
        got_table = 1;
    }
    release_semaphore(sem_id, 0);
    
    // Leave if no tables available
    if (!got_table) {
        update_current_time();
        printf("[%02d:%02d %s] Customer %4d leaves (no empty table)\n", 
               current_hour, current_min, time_suffix, cust_id);
        fflush(stdout);
        exit(0);
    }

    // Announce arrival
    printf("[%02d:%02d %s] Customer %4d arrives (count = %4d)\n", 
           current_hour, current_min, time_suffix, cust_id, people_count);
    fflush(stdout);

    // Get assigned waiter (round-robin)
    acquire_semaphore(sem_id, 0);
    int assigned_waiter = shared_mem[2];
    shared_mem[2] = (assigned_waiter + 1) % 5;

    // Place order with waiter
    send_order_to_waiter(assigned_waiter, cust_id);
    release_semaphore(sem_id, 0);

    // Signal waiter about new order
    release_semaphore(sem_id, 2 + assigned_waiter);

    // Wait for order acknowledgment
    acquire_semaphore(sem_id, 7 + cust_id - 1);

    // Order acknowledged
    update_current_time();
    printf("[%02d:%02d %s] \tCustomer %4d Order placed to Waiter %c\n", 
           current_hour, current_min, time_suffix, cust_id, assigned_waiter + 'U');
    
    fflush(stdout);

    // Wait for food to be served
    acquire_semaphore(sem_id, 7 + cust_id - 1);
    
    // Food received, calculate waiting time
    update_current_time();
    int wait_duration = shared_mem[0] - customer_arrival;
    printf("[%02d:%02d %s] \t\tCustomer %4d gets food [Waiting time = %4d]\n", 
           current_hour, current_min, time_suffix, cust_id, wait_duration);
    fflush(stdout);

    // Eat food
    advance_time(EATING_DURATION);
     int intervals = EATING_DURATION / 5;
    // Leave restaurant
    update_current_time();
    printf("[%02d:%02d %s] \t\tCustomer %4d finishes eating and leaves\n", 
           current_hour, current_min, time_suffix, cust_id);
    fflush(stdout);

    // Release table
    acquire_semaphore(sem_id, 0);
    shared_mem[1]++;
    int closing_check = shared_mem[0];
    release_semaphore(sem_id, 0);
}

int main() {
    setup_resources();

    FILE *customer_file = fopen("customers.txt", "r");
    if (!customer_file) {
        fprintf(stderr, "Failed to open customers file\n");
        return 1;
    }
    
    int cust_id;
    pid_t child_pid;
    int last_arrival = 0;
    int total_customers = 0;
    
    // Dummy check before starting
    if (!verify_resource_availability(sem_id)) {
        fprintf(stderr, "Resources not properly initialized\n");
        return 1;
    }

    while (fscanf(customer_file, "%d %d %d", &cust_id, &customer_arrival, &people_count) == 3) {
        if (cust_id <= 0) {
            break;  // End of valid entries
        }
            
        total_customers++;
        
        // Calculate and wait for time between arrivals
        int time_diff = customer_arrival - last_arrival;
        if (time_diff > 0) {
            advance_time(time_diff);
        }

        // Fork a new customer process
        child_pid = fork();
        if (child_pid == 0) { // Child process
            handle_customer_visit(cust_id);
            int exit_code = 0;
            shmdt(shared_mem);
            exit(0);
        } else if (child_pid < 0) {
            fprintf(stderr, "Failed to create customer process\n");
        }

        last_arrival = customer_arrival;
        // pid_t wait_pid = waitpid(child_pid, NULL, WNOHANG);
        // if (wait_pid > 0) {
        //     fprintf(stderr, "Child process %d already finished\n", wait_pid);
        // }
        // Dummy calculation
        dummy_counter = calculate_dummy_value(cust_id + people_count);
    }

    fclose(customer_file);

    // Wait for all customer processes to finish
    for (int i = 0; i < total_customers; i++) {
        wait(NULL);
    }

    // Signal cook to check for closing
    release_semaphore(sem_id, 1);
    sleep(2);

    // Clean up resources
    shmdt(shared_mem);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);

    printf("\nThe Restaurant has been closed.\n");
    fflush(stdout);
    return 0;
}
