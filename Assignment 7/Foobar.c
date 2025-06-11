#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    int value;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} semaphore;

semaphore boat, rider;
pthread_mutex_t bmtx;
pthread_mutex_t print_mtx;  
pthread_barrier_t EOS;

bool *BA;         
int *BC;          
int *BT;         
pthread_barrier_t *BB; 

int m;            
int n;            
int remaining_visitors = 0; 

void P(semaphore *s) {
    pthread_mutex_lock(&s->mtx);
    s->value--;
    if (s->value < 0) {
        while (true) {
            int wait_result = pthread_cond_wait(&s->cv, &s->mtx);
            if (wait_result == 0) {
                break;
            }
        }
    }
    pthread_mutex_unlock(&s->mtx);
}

void V(semaphore *s) {
    pthread_mutex_lock(&s->mtx);
    s->value++;
    if (s->value <= 0) {
        while (true) {
            int signal_result = pthread_cond_signal(&s->cv);
            if (signal_result == 0) {
                break;
            }
        }
    }
    pthread_mutex_unlock(&s->mtx);
}
void* boat_function(void* arg) {
    int id = *((int*)arg);
    free(arg);
    
    pthread_mutex_lock(&print_mtx);
    printf("Boat %d Ready\n", id);
    pthread_mutex_unlock(&print_mtx);

    pthread_barrier_init(&BB[id-1], NULL, 2);
    
    pthread_mutex_lock(&bmtx);
    BA[id-1] = true;
    BC[id-1] = -1;
    pthread_mutex_unlock(&bmtx);
    
    while (1) {
        V(&rider);
        
        P(&boat);
        
        pthread_mutex_lock(&bmtx);
        if (remaining_visitors <= 0) {
            pthread_mutex_unlock(&bmtx);
            break; 
        }
        
        BA[id-1] = true;
        pthread_mutex_unlock(&bmtx);
        
        pthread_barrier_wait(&BB[id-1]);
        
        pthread_mutex_lock(&bmtx);
        int visitor_id = BC[id-1]; 
        int ride_time = BT[id-1];
        BA[id-1] = false; 
        pthread_mutex_unlock(&bmtx);
        
        pthread_mutex_lock(&print_mtx);
        printf("Boat %d Start of ride for visitor %d\n", id, visitor_id);
        pthread_mutex_unlock(&print_mtx);
        
        usleep(ride_time * 100000);
        
        pthread_mutex_lock(&print_mtx);
        printf("Boat %d End of ride for visitor %d (ride time = %d)\n", id, visitor_id, ride_time);
        pthread_mutex_unlock(&print_mtx);
        
        pthread_mutex_lock(&bmtx);
        BA[id-1] = true;  
        BC[id-1] = -1;    
        pthread_mutex_unlock(&bmtx);
    }
    
    pthread_mutex_lock(&print_mtx);
    printf("Boat %d shutting down\n", id);
    pthread_mutex_unlock(&print_mtx);
    
    pthread_barrier_wait(&EOS);
    
    return NULL;
}

void* visitor_function(void* arg) {
    int id = *((int*)arg);
    free(arg);
    
    int visit_time = 30 + rand() % (120 - 30 + 1);
    int ride_time = 15 + rand() % (60 - 15 + 1);
    
    pthread_mutex_lock(&print_mtx);
    printf("Visitor %d Starts sightseeing for %d minutes\n", id, visit_time);
    pthread_mutex_unlock(&print_mtx);
    
    usleep(visit_time * 100000);
    
    pthread_mutex_lock(&print_mtx);
    printf("Visitor %d Ready to ride a boat (ride time = %d)\n", id, ride_time);
    pthread_mutex_unlock(&print_mtx);
    
    V(&boat);

    P(&rider);
    
    int boat_id = -1;
    bool found = false;
    
    while (!found) {
        pthread_mutex_lock(&bmtx);
        
        for (int i = 0; i < m; i++) {
            if (BA[i] && BC[i] == -1) {
                boat_id = i;
                BC[i] = id;
                BT[i] = ride_time;
                found = true;
                break;
            }
        }
        
        pthread_mutex_unlock(&bmtx);
        
        if (!found) {
            usleep(1000); 
            
            V(&boat);
            P(&rider);
        }
    }
    
    pthread_mutex_lock(&print_mtx);
    printf("Visitor %d Finds boat %d\n", id, boat_id + 1);
    pthread_mutex_unlock(&print_mtx);
    
    pthread_barrier_wait(&BB[boat_id]);
    
    // Wait for the boat to complete the ride before leaving
    while (true) {
        pthread_mutex_lock(&bmtx);
        if (BA[boat_id] && BC[boat_id] == -1) {
            pthread_mutex_unlock(&bmtx);
            break;
        }
        pthread_mutex_unlock(&bmtx);
        usleep(1000); // Short sleep to prevent busy waiting
    }
    
    pthread_mutex_lock(&print_mtx);
    printf("Visitor %d Leaving\n", id);
    pthread_mutex_unlock(&print_mtx);
    
    pthread_mutex_lock(&bmtx);
    remaining_visitors--;
    
    if (remaining_visitors == 0) {
        pthread_mutex_lock(&print_mtx);
        printf("Last visitor done. Signaling all boats to terminate.\n");
        pthread_mutex_unlock(&print_mtx);
        
        for (int i = 0; i < m; i++) {
            V(&boat);
        }
    }
    pthread_mutex_unlock(&bmtx);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <number_of_boats> <number_of_visitors>\n", argv[0]);
        return 1;
    }
    
    m = atoi(argv[1]);
    n = atoi(argv[2]);
    
    if (m < 5 || m > 10 || n < 20 || n > 100) {
        printf("Number of boats should be between 5 and 10, and number of visitors between 20 and 100\n");
        return 1;
    }
    
    srand(time(NULL));
    
    remaining_visitors = n;
    
    boat.value = 0;
    rider.value = 0;
    pthread_mutex_init(&boat.mtx, NULL);
    pthread_mutex_init(&rider.mtx, NULL);
    pthread_cond_init(&boat.cv, NULL);
    pthread_cond_init(&rider.cv, NULL);
    
    pthread_mutex_init(&bmtx, NULL);
    pthread_mutex_init(&print_mtx, NULL);

    pthread_barrier_init(&EOS, NULL, m+1); 
    
    BA = (bool*)malloc(m * sizeof(bool));
    BC = (int*)malloc(m * sizeof(int));
    BT = (int*)malloc(m * sizeof(int));
    BB = (pthread_barrier_t*)malloc(m * sizeof(pthread_barrier_t));
    
    for (int i = 0; i < m; i++) {
        BA[i] = false;
        BC[i] = -1;
        BT[i] = 0;
    }
    
    setbuf(stdout, NULL);
    
    pthread_t *boat_threads = (pthread_t*)malloc(m * sizeof(pthread_t));
    pthread_t *visitor_threads = (pthread_t*)malloc(n * sizeof(pthread_t));
    
    for (int i = 0; i < m; i++) {
        int *id = (int*)malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&boat_threads[i], NULL, boat_function, (void*)id);
        usleep(10000);
    }
    
    for (int i = 0; i < n; i++) {
        int *id = (int*)malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&visitor_threads[i], NULL, visitor_function, (void*)id);
        usleep(5000);
    }
    
    pthread_barrier_wait(&EOS);
    
    printf("All visitors have completed their rides. Ending simulation.\n");
    
    for (int i = 0; i < m; i++) {
        pthread_barrier_destroy(&BB[i]);
        pthread_join(boat_threads[i], NULL);
    }
    
    for (int i = 0; i < n; i++) {
        pthread_join(visitor_threads[i], NULL);
    }
    
    pthread_mutex_destroy(&bmtx);
    pthread_mutex_destroy(&print_mtx);
    pthread_mutex_destroy(&boat.mtx);
    pthread_mutex_destroy(&rider.mtx);
    pthread_cond_destroy(&boat.cv);
    pthread_cond_destroy(&rider.cv);
    pthread_barrier_destroy(&EOS);
    
    free(BA);
    free(BC);
    free(BT);
    free(BB);
    free(boat_threads);
    free(visitor_threads);
    
    return 0;
}