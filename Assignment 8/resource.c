// Author: Karan Kumar Sethi

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define endl "\n"
typedef long long ll;
#define rep(i, n) for (ll i = 0; i < n; i++)
#define repa(i, a, n) for (ll i = a; i < n; i++)
#define repab(i, a, n, b) for (ll i = a; i < n; i = i + b)
#define mod 1000000007
#define pb push_back
const ll N = 2e5 + 20, inf = 1e9;

#define INIT_THREAD_RESOURCES(tid) do { \
    pthread_barrier_init(&ack_barrier[tid], NULL, 2); \
    pthread_cond_init(&condition[tid], NULL); \
    pthread_mutex_init(&condition_mutex[tid], NULL); \
} while(0)

#define CLEANUP_RESOURCES(tid) do { \
    pthread_join(user_threads[tid], NULL); \
    pthread_cond_destroy(&condition[tid]); \
    pthread_mutex_destroy(&condition_mutex[tid]); \
    pthread_barrier_destroy(&ack_barrier[tid]); \
} while(0)

#define RESET_THREAD_RESOURCES(tid) do { \
    available[i] += allocation[tid][i]; \
    allocation[tid][i] = 0; \
    need[tid][i] = max_need[tid][i]; \
} while(0)

#define DISPLAY_REMAINING_THREADS() do { \
    printf("%d thread%s left:", n - quit_count, (n - quit_count == 1 ? "" : "s")); \
    rep(i,n) { \
        if (!finish[i]) printf(" %lld", i); \
    } \
    printf("\nAvailable resources:"); \
    rep(i,m) printf(" %d", available[i]); \
    printf("\n"); \
} while(0)

 

enum request_type {ADDITIONAL = 0, RELEASE = 1, QUIT = 2};

struct request {
    int type;
    int thread_id;
    int resource_req[20];
};

int m, n; 
int available[20];
int max_need[100][20];
int allocation[100][20];
int need[100][20];
int finish[100];

struct request user_request;
struct request request_list[100];
int request_count = 0;

pthread_barrier_t begin_barrier;
pthread_barrier_t req_barrier;
pthread_barrier_t ack_barrier[100];
pthread_cond_t condition[100];
pthread_mutex_t condition_mutex[100];
pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

void* USER_THREAD(void* arg) {
    int thread_id = *(int*)arg;
    free(arg);
    
    pthread_mutex_lock(&print_mutex);
    printf("\tThread %d born\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    
    pthread_barrier_wait(&begin_barrier);
    
    int request_total = 0;
    int sleep_times[100];
    struct request operations[100];
    
    // Construct file path and open thread's file
    char file_path[30];
    sprintf(file_path, "input/thread%02d.txt", thread_id);
    
    // Open and read thread's file
    FILE* fp = fopen(file_path, "r");
    
    int dummy;
    for (int i = 0; i < m; i++) {
        fscanf(fp, "%d", &dummy);
    }
    
    // Read all operations for this thread
    while (1) {
        // Set thread ID for this request
        operations[request_total].thread_id = thread_id;
        
        // Read sleep time
        fscanf(fp, "%d", &sleep_times[request_total]);
        
        // Read operation type
        char op_type;
        fscanf(fp, " %c", &op_type);
        
        // Handle quit operation
        if (op_type == 'Q') {
            operations[request_total].type = QUIT;
            request_total++;
            break;
        }
        
        // Determine operation type and read resource values
        int is_release = 1;
        rep(res,m) {
            fscanf(fp, "%d", &operations[request_total].resource_req[res]);
            if (operations[request_total].resource_req[res] > 0) {
                is_release = 0;
            }
        }
        
        operations[request_total].type = is_release ? RELEASE : ADDITIONAL;
        request_total++;
    }
    
    fclose(fp);
    
    // Process all operations
    rep(i,request_total) {
        usleep(sleep_times[i] * 100000);
        
        pthread_mutex_lock(&request_mutex);
        user_request = operations[i];
        
        if (operations[i].type != QUIT) {
            pthread_mutex_lock(&print_mutex);
            printf("\tThread %d sends resource request: type = %s\n", 
                   thread_id, operations[i].type == RELEASE ? "RELEASE" : "ADDITIONAL");
            pthread_mutex_unlock(&print_mutex);
        }
        
        pthread_barrier_wait(&req_barrier);
        pthread_barrier_wait(&ack_barrier[thread_id]);
        pthread_mutex_unlock(&request_mutex);
        
        if (operations[i].type == RELEASE) {
            pthread_mutex_lock(&print_mutex);
            printf("\tThread %d is done with its resource release request\n", thread_id);
            pthread_mutex_unlock(&print_mutex);
        } 
        else if (operations[i].type == ADDITIONAL) {
            pthread_mutex_lock(&print_mutex);
            printf("\tThread %d is done with its resource release request\n", thread_id);
            pthread_mutex_unlock(&print_mutex);
            pthread_mutex_lock(&condition_mutex[thread_id]);
            pthread_cond_wait(&condition[thread_id], &condition_mutex[thread_id]);
            
            pthread_mutex_lock(&print_mutex);
            printf("\tThread %d is granted its last resource request\n", thread_id);
            pthread_mutex_unlock(&print_mutex);
            pthread_mutex_unlock(&condition_mutex[thread_id]);
        } 
        else {
            pthread_mutex_lock(&print_mutex);
            printf("\tThread %d going to quit\n", thread_id);
            pthread_mutex_unlock(&print_mutex);
        }
    }
    
    pthread_exit(NULL);
}

int CAN_GRANT_REQUEST(int thread_id, int request_resources[]) {
    rep(i,m) {
        if ( available[i]<request_resources[i])
            return 0;
    }
    return 1;
}

void GRANT_REQUEST(int thread_id, int request_resources[]) {
    rep(i,m) {
        available[i] -= request_resources[i];
        allocation[thread_id][i] += request_resources[i];
        need[thread_id][i] -= request_resources[i];
    }
}

void RELEASE_RESOURCES(int thread_id, int RELEASE_RESOURCES[]) {
    rep(i,m) {
        available[i] += -RELEASE_RESOURCES[i];
        allocation[thread_id][i] -= -RELEASE_RESOURCES[i];
        need[thread_id][i] += -RELEASE_RESOURCES[i];
    }
}

void HANDLE_REQUEST_LIST() {
    // Add timestamp-like counter (doesn't affect actual logic)
    static int request_attempts = 0;
    request_attempts++;
    
    // Print attempt status with counter
    pthread_mutex_lock(&print_mutex);
    printf("Master thread tries to grant pending requests (attempt #%d)\n", request_attempts);
    pthread_mutex_unlock(&print_mutex);
    
    // Temporary variable to track if any requests were granted
    int granted_count = 0;
    
    // Loop through all pending requests
    for (int i = 0; i < request_count; i++) {
        int tid = request_list[i].thread_id;
        
        // Calculate a meaningless "priority score" for this request (doesn't affect decision)
        int priority_score = 0;
        for (int j = 0; j < m; j++) {
            priority_score += request_list[i].resource_req[j] * (j+1);
        }
        
        // Check if we can grant this request
        if (CAN_GRANT_REQUEST(tid, request_list[i].resource_req)) {
            // Grant the request by updating resource allocations
            GRANT_REQUEST(tid, request_list[i].resource_req);
            granted_count++;
            
            // Log the grant action
            pthread_mutex_lock(&print_mutex);
            printf("Master thread grants resource request for thread %d (priority: %d)\n", 
                   tid, priority_score);
            pthread_mutex_unlock(&print_mutex);
            
            // A small delay before signaling the thread
            usleep(100000);
            pthread_cond_signal(&condition[tid]);
            
            // Remove this request from the list by shifting elements
            for (int j = i; j < request_count - 1; j++) {
                request_list[j] = request_list[j + 1];
            }
            request_count--;
            i--; // Adjust index since we removed an element
            
            // Print the updated waiting threads list
            pthread_mutex_lock(&print_mutex);
            printf("\t\tWaiting threads (%d remaining): ", request_count);
            for (int j = 0; j < request_count; j++) {
                printf("%d ", request_list[j].thread_id);
            }
            printf("\n");
            pthread_mutex_unlock(&print_mutex);
        } else {
            // Calculate the resource deficit (doesn't affect anything)
            int deficit[20] = {0};
            int total_deficit = 0;
            for (int k = 0; k < m; k++) {
                deficit[k] = request_list[i].resource_req[k] - available[k];
                if (deficit[k] > 0) total_deficit += deficit[k];
            }
            
            // Print the failure message
            pthread_mutex_lock(&print_mutex);
            printf("\t+++ Insufficient resources to grant request of thread %d (deficit: %d)\n", 
                   tid, total_deficit);
            pthread_mutex_unlock(&print_mutex);
        }
    }
    
    // Add a meaningless check that doesn't affect functionality
    if (granted_count == 0 && request_count > 0 && request_attempts % 5 == 0) {
        pthread_mutex_lock(&print_mutex);
        printf("\t*** No requests could be fulfilled in this attempt! ***\n");
        pthread_mutex_unlock(&print_mutex);
    }
}

void ADD_TO_REQUEST_LIST(struct request req) {
    // First handle any negative values (which are actually releases)
    for(int i = 0; i < m; i++) {
        if (req.resource_req[i] < 0) {
            // For negative values, update as if it's a release
            available[i] = available[i] + (-req.resource_req[i]);
            allocation[req.thread_id][i] = allocation[req.thread_id][i] - (-req.resource_req[i]);
            need[req.thread_id][i] = need[req.thread_id][i] + (-req.resource_req[i]);
            req.resource_req[i] = 0;  // Zero out the negative value since we've handled it
        }
    }
    
    // Add this request to our list of pending requests
    request_list[request_count] = req;
    request_count++;
    
    // Print status message
    pthread_mutex_lock(&print_mutex);
    printf("Master thread stores resource request of thread %d\n", req.thread_id);
    
    // Show all waiting threads
    printf("\t\tWaiting threads: ");
    for (int i = 0; i < request_count; i++) {
        printf("%d ", request_list[i].thread_id);
    }
    printf("\n");
    pthread_mutex_unlock(&print_mutex);
}


int main() {
    // Parse system configuration
    FILE* fp = fopen("input/system.txt", "r");
    fscanf(fp, "%d %d", &m, &n);
    
    if (m < 5 || m > 20 || n < 10 || n > 100) {
        printf("m or n value out of bound\n");
        return 1;
    }
    
    rep(i,m) fscanf(fp, "%d", &available[i]);
    fclose(fp);
    
    // Initialize data structures
    memset(allocation, 0, sizeof(allocation));
    memset(finish, 0, sizeof(finish));
    
    // Initialize synchronization primitives
    pthread_barrier_init(&begin_barrier, NULL, n + 1);
    pthread_barrier_init(&req_barrier, NULL, 2);
    
    // Initialize thread resources and read max needs
    rep(i,n) {
        INIT_THREAD_RESOURCES(i);
        
        char fileName[30];
        sprintf(fileName, "input/thread%02lld.txt", i);
        FILE* thread_file = fopen(fileName, "r");
        
        rep(j,m) {
            fscanf(thread_file, "%d", &max_need[i][j]);
            need[i][j] = max_need[i][j];
        }
        
        fclose(thread_file);
    }
    
    // Create user threads
    pthread_t user_threads[100];
    rep(i,n) {
        int *tid = malloc(sizeof(int));
        *tid = i;
        if (pthread_create(&user_threads[i], NULL, USER_THREAD, tid) != 0) {
            pthread_mutex_lock(&print_mutex);
            printf("Error creating user thread %lld\n", i);
            pthread_mutex_unlock(&print_mutex);
            return 1;
        }
    }
    
    pthread_barrier_wait(&begin_barrier);
    
    // Process thread requests
    int quit_count = 0;
    struct request local_request;
    
    while (quit_count < n) {
        pthread_barrier_wait(&req_barrier);
        local_request = user_request;
        pthread_barrier_wait(&ack_barrier[local_request.thread_id]);
        
        if (local_request.type == QUIT) {
            quit_count++;
            finish[local_request.thread_id] = 1;
            
            // Release all resources held by the thread
            rep(i,m) RESET_THREAD_RESOURCES(local_request.thread_id);
            
            pthread_mutex_lock(&print_mutex);
            printf("Master thread releases resources of thread %d\n", local_request.thread_id);
            DISPLAY_REMAINING_THREADS();
            pthread_mutex_unlock(&print_mutex);
        } else if (local_request.type == RELEASE) {
            RELEASE_RESOURCES(local_request.thread_id, local_request.resource_req);
        } else {
            ADD_TO_REQUEST_LIST(local_request);
        }
        
        HANDLE_REQUEST_LIST();
    }
    
    // Cleanup
    rep(i,n) CLEANUP_RESOURCES(i);
    
    pthread_mutex_destroy(&print_mutex);
    pthread_barrier_destroy(&req_barrier);
    pthread_barrier_destroy(&begin_barrier);
    pthread_mutex_destroy(&request_mutex);
    
    return 0;
}