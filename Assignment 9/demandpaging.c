// AUTHOR : KARAN KUMAR SETHI
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

typedef long long ll;
#define rep(i, n) for (ll i = 0; i < n; i++)
#define repa(i, a, n) for (ll i = a; i < n; i++)
#define repab(i, a, n, b) for (ll i = a; i < n; i = i + b)
const int PAGE_SIZE = 4096 ;          
const int INTS_PER_PAGE = PAGE_SIZE/4 ; 

const int TOTAL_MEMORY_MB = 64;
const int OS_MEMORY_MB = 16 ;
const int USER_MEMORY_BYTES = ((TOTAL_MEMORY_MB - OS_MEMORY_MB)*1024*1024);
const int TOTAL_FRAMES = (USER_MEMORY_BYTES / PAGE_SIZE) ;

#define VALID_MASK 0x8000
#define FRAME_MASK 0x7FFF
typedef uint16_t pte_t;

int *free_frames;
int free_frame_top = 0;  
unsigned long total_page_accesses = 0;
unsigned long total_page_faults = 0;
unsigned long total_swaps = 0;
int min_active_processes = 1e9;  

typedef struct {
    int pid;        
    int s;               
    int m;              
    int current_search;  
    int *search_keys;   
    pte_t page_table[2048];  
    bool swapped;        
} Process;

typedef struct QueueNode {
    Process *proc;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *head, *tail;
    int count;
} Queue;

// adds process to queue
void enqueue(Queue *q, Process *proc) {
    QueueNode *node = malloc(sizeof(QueueNode));
    node->proc = proc;
    node->next = NULL;
    if(q->tail)
        q->tail->next = node;
    else
        q->head = node;
    q->tail = node;
    q->count++;
}

// removes process from queue
Process *dequeue(Queue *q) {
    if(q->head == NULL) return NULL;
    QueueNode *node = q->head;
    Process *proc = node->proc;
    q->head = node->next;
    if(q->head == NULL) q->tail = NULL;
    free(node);
    q->count--;
    return proc;
}

// sets up the free frames 
void init_free_frames() {
    free_frames = malloc(TOTAL_FRAMES * sizeof(int));
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        free_frames[i] = i;
    }
    free_frame_top = TOTAL_FRAMES;
}

// loads essential page for process
bool load_essential_page(Process *p, int pt_index) {
   if(free_frame_top == 0) return false; 
   int frame = free_frames[--free_frame_top];
   p->page_table[pt_index] = (pte_t)(VALID_MASK | (frame & FRAME_MASK));
   return true;
}

// loads page for process
bool load_page(Process *p, int pt_index) {
   if(free_frame_top == 0) return false;
   int frame = free_frames[--free_frame_top];
   p->page_table[pt_index] = (pte_t)(VALID_MASK | (frame & FRAME_MASK));
   return true;
}

// frees the frames used by a process
void free_process_frames(Process *p) {
    rep(i,2048) {
        if(p->page_table[i] & VALID_MASK) {
            int frame = p->page_table[i] & FRAME_MASK;
           free_frames[free_frame_top++] = frame;
            p->page_table[i] = 0;
        }
    }
}

// does binary search simulation
bool simulate_binary_search(Process *p) {
   // Get the key we're searching for
   int key = p->search_keys[p->current_search];
   // Set up the search boundaries
   int left = 0;
   int right = p->s - 1;

   // Keep searching while there's a valid range
   while (left < right) {
       // Find the middle point
       int middle = (left + right) / 2;
       
       // Count this as a page access
       total_page_accesses++;
       
       // Figure out which page has this element
       int page_index = 10 + (middle / INTS_PER_PAGE);
       
       // Check if the page is loaded
       if ((p->page_table[page_index] & VALID_MASK) == 0) {
           // Page fault happened
           total_page_faults++;
           
           // Try to load the page
           if (!load_page(p, page_index)) {
               // No free frames available
               return false;
           }
       }
       
       // Update search boundaries based on comparison
       if (key <= middle) {
           right = middle;
       } else {
           left = middle + 1;
       }
   }

   // Search completed successfully
   return true;
}

// swap in a process from disk to memory
void swap_in(Process *p, Queue *ready_queue, int active_count) {
   
    rep(i,10) {
    load_essential_page(p, i);
    }
    p->swapped = false;
    printf("+++ Swapping in process %4d  [%d active processes]\n", p->pid, active_count);
    enqueue(ready_queue, p);
}

int main(void) {
    FILE *fp = fopen("search.txt", "r");
    int n, m;
    fscanf(fp, "%d %d", &n, &m);
    
    // Allocate processes array
    Process **procs = malloc(n * sizeof(Process *));
    rep(i, n) {
        procs[i] = malloc(sizeof(Process));
        procs[i]->pid = i;
        procs[i]->current_search = 0;
        procs[i]->m = m;
        fscanf(fp, "%d", &procs[i]->s);
        procs[i]->search_keys = malloc(m * sizeof(int));
        for (int j = 0; j < m; j++) {
            fscanf(fp, "%d", &procs[i]->search_keys[j]);
        }
        // Initialize page table: essential pages (first 10) will be loaded now.
        rep(j, 2048) {
            procs[i]->page_table[j] = 0;
        }
        procs[i]->swapped = false;
    }
    fclose(fp);
    
    // Initialize free frame pool.
    init_free_frames();
    
    // Initialize ready queue and swapped-out queue.
    Queue ready_queue = {0};
    Queue swapped_out_queue = {0};
    
    // Initially load all processes with their 10 essential pages and add to ready queue.
    
    printf("+++ Simulation data read from file\n+++ Kernel data initialized\n");

    rep(i, n) {
        rep(j, 10) {
            if (!load_essential_page(procs[i], j)) {
                fprintf(stderr, "Error: not enough memory to load process %d essential page %lld\n", procs[i]->pid, j);
                exit(1);
            }
        }
        enqueue(&ready_queue, procs[i]);
    }
    
    // Simulation loop: continue until all processes have finished.
    int active_processes = n; // number of processes that have not terminated
    while (active_processes > 0) {
        // If ready queue is empty, then nothing to schedule (should not happen normally)
        if (ready_queue.count == 0) {
            // If there is a process waiting to be swapped in, do that.
            if (swapped_out_queue.count > 0) {
                Process *p = dequeue(&swapped_out_queue);
                swap_in(p, &ready_queue, ready_queue.count + 1);
            } else {
                break;
            }
        }
        
        Process *p = dequeue(&ready_queue);
        // If process is already finished, skip
        if (p->current_search >= p->m) continue;
        
        // Run one binary search quantum.
        bool finished_search = simulate_binary_search(p);
        if (!finished_search) {
            // A page fault occurred when no free frames were available:
            // active processes are those currently in ready queue plus the running one.
            int current_active = ready_queue.count ;
            if (current_active < min_active_processes)
                min_active_processes = current_active;
            // Swap out the process.
           total_swaps++;
           printf("+++ Swapping out process %4d  [%d active processes]\n", p->pid, current_active);
           free_process_frames(p);
           p->swapped = true;
           enqueue(&swapped_out_queue, p);
            // Do not enqueue it; its current search will be restarted upon swap-in.
        } else {
            // Search finished successfully.
            p->current_search++;
       #ifdef VERBOSE
            printf("Search %d by Process %d\n",p->current_search, p->pid);
       #endif
            if (p->current_search < p->m) {
                enqueue(&ready_queue, p);
            } else {
                free_process_frames(p);
                active_processes--;
                if (swapped_out_queue.count > 0) {
                    Process *q = dequeue(&swapped_out_queue);
                    swap_in(q, &ready_queue, ready_queue.count + 1);
                }
            }
        }
        
        if (free_frame_top == 0) {
            int current_active = ready_queue.count + 1;
            if (current_active < min_active_processes)
                min_active_processes = current_active;
        }
    }
    
    printf("\n+++ Page access summary\n\n");
    printf("Total number of page accesses  =  %lu\n", total_page_accesses);
    printf("Total number of page faults    =  %lu\n", total_page_faults);
    printf("Total number of swaps          =  %lu\n", total_swaps);
    printf("Degree of multiprogramming     =  %d\n", (min_active_processes == 1e9 ? n : min_active_processes));
    
    rep(i, n) {
        free(procs[i]->search_keys);
        free(procs[i]);
    }
    free(procs);
    free(free_frames);
    
    return 0;
}