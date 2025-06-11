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

#define CLOSING_HOUR 240
#define TIME_NEEDED_TO_COOK_PER_PERSON 5

/* Data structures */
union sem_stuff {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/* Global variables */
int g_shmid;
int g_semid;
int *g_memory;
int g_min = 0;
int g_hr = 0;
char *g_period = "AM";
int g_debug_level = 0;

/* Function prototypes */
void do_sem_op(int sid, int snum, int operation);
void sem_down(int sid, int snum);
void sem_up(int sid, int snum);
void refresh_clock();
void sleep_for(int wait_mins);
void tell_waiter(int w_id, int c_id);
void chef_work(int chef_id);
void init_resources();
void debug_print(int level, const char *msg);
int rand_delay(int base, int variance);
void cleanup_resources();

/* Debug function */
void debug_print(int level, const char *msg) {
    if (g_debug_level >= level) {
        printf("DEBUG[%d]: %s\n", level, msg);
        fflush(stdout);
    }
}

/* Random time delay calculator */
int rand_delay(int base, int variance) {
    return base + (rand() % variance);
}

/* Semaphore operations */
void do_sem_op(int sid, int snum, int operation) {
    struct sembuf sb;
    sb.sem_op = operation;
    sb.sem_flg = 0;
    sb.sem_num = snum;
    
    if (semop(sid, &sb, 1) == -1) {
        perror("semop error");
        exit(1);
    }
}

void sem_down(int sid, int snum) {
    do_sem_op(sid, snum, -1);
}

void sem_up(int sid, int snum) {
    do_sem_op(sid, snum, 1);
}

/* Time management functions */
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
    int curr_time = g_memory[0];
    int future = curr_time + wait_mins;
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

void tell_waiter(int w_id, int c_id) {
    int pos = 100 + (w_id * 200);
        
    sem_down(g_semid, 0);
    g_memory[pos] = c_id;
    sem_up(g_semid, 0);
    sem_up(g_semid, 2 + w_id);
}

/* Food preparation function */
void make_food(int chef_id, int w_id, int c_id, int party_size) {
    refresh_clock();
    
    char chef_letter = (chef_id == 0) ? 'C' : 'D';
    
    printf("[%02d:%02d %s] Chef %c: Making food (Waiter %c, Customer %4d, Group %4d)\n", 
           g_hr, g_min, g_period, chef_letter, w_id + 'U', c_id, party_size);
    fflush(stdout);
    
    int cook_time = party_size * 5;
    
    // Add some randomness to cooking time
    cook_time = rand_delay(cook_time, 2);
    
    sleep_for(cook_time);
    
    refresh_clock();
    
    printf("[%02d:%02d %s] Chef %c: Food ready  (Waiter %c, Customer %4d, Group %4d)\n", 
           g_hr, g_min, g_period, chef_letter, w_id + 'U', c_id, party_size);
    fflush(stdout);
    
    tell_waiter(w_id, c_id);
}

/* Check if restaurant should close */
int is_closing() {
    int front = g_memory[1100];
    int back = g_memory[1101];
    
    if (front == back && g_memory[0] > CLOSING_HOUR) {
        return 1;
    } else {
        return 0;
    }
}

/* Core chef process implementation */
void chef_work(int chef_id) {
    refresh_clock();
    
    char chef_letter = (chef_id == 0) ? 'C' : 'D';
    printf("[%02d:%02d %s] Chef %c arrived.\n", g_hr, g_min, g_period, chef_letter);
    fflush(stdout);
    
    while (1) {
        sem_down(g_semid, 1);
        sem_down(g_semid, 0);
        
        int front = g_memory[1100];
        int back = g_memory[1101];
        
        if (is_closing()) {
            sem_up(g_semid, 0);
            break;
        } else {
            int w_id = g_memory[front];
            int c_id = g_memory[front + 1];
            int party_size = g_memory[front + 2];
            
            g_memory[1100] = front + 3;
            sem_up(g_semid, 0);
            
            make_food(chef_id, w_id, c_id, party_size);
            
            sem_down(g_semid, 0);
            if (is_closing()) {
                sem_up(g_semid, 0);
                break;
            }
            sem_up(g_semid, 0);
        }
    }
    
    refresh_clock();
    
    printf("[%02d:%02d %s] Chef %c: Going home.\n", g_hr, g_min, g_period, chef_letter);
    fflush(stdout);
    
    sem_down(g_semid, 0);
    if (g_memory[3] == 1) {
        int i;
        for (i = 0; i < 5; i++) {
            sem_up(g_semid, i + 2);
        }
    }
    g_memory[3]--;
    sem_up(g_semid, 0);
}

/* Clean up resources */
void cleanup_resources() {
    shmdt(g_memory);
    debug_print(1, "Shared memory detached");
}

/* Setup functions */
void init_resources() {
    key_t shm_key = ftok("makefile", 'B');
    key_t sem_key = ftok("makefile", 'A');
    union sem_stuff semun;
    unsigned short vals[207];
    int i;
    g_shmid = shmget(shm_key, 2001 * sizeof(int), IPC_CREAT | 0666);
    
    g_memory = (int *)shmat(g_shmid, NULL, 0);
    
    g_memory[0] = 0;     
    g_memory[1] = 10;    
    g_memory[2] = 0;     
    g_memory[3] = 2;     
    g_memory[1100] = 1102;  
    g_memory[1101] = 1102;  
    
    g_semid = semget(sem_key, 207, IPC_CREAT | 0666);
    vals[0] = 1;  
    for (i = 1; i < 207; i++) {
        vals[i] = 0;  
    }
    
    semun.array = vals;
    semctl(g_semid, 0, SETALL, semun);
    debug_print(1, "IPC resources initialized");
}

int main(int argc, char *argv[]) {
    pid_t pid;
    int i;
    
    if (argc > 1) {
        g_debug_level = atoi(argv[1]);
    }
    
    // Seed random number generator
    srand(time(NULL));
    
    init_resources();
    
    for (i = 0; i < 2; i++) {
        pid = fork();
        if (pid == 0) {
            chef_work(i);
            cleanup_resources();
            exit(0);
        }
    }
    
    for (i = 0; i < 2; i++) {
        wait(NULL);
    }
    
    cleanup_resources();
    printf("\nThe restaurant is closed.\n");
    fflush(stdout);
    
    return 0;
}
