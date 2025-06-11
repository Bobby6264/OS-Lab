// AUTHOR: KARAN KUMAR SETHI
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <limits.h>
#include <string.h>

typedef long long ll;
#define rep(i, n) for (ll i = 0; i < n; i++)
#define repa(i, a, n) for (ll i = a; i < n; i++)
#define repab(i, a, n, b) for (ll i = a; i < n; i = i + b)

const int PAGE_SIZE = 4096;          
const int INTS_PER_PAGE = PAGE_SIZE/4; 
const int TOTAL_MEMORY_MB = 64;
const int OS_MEMORY_MB = 16;
const int USER_MEMORY_BYTES = ((TOTAL_MEMORY_MB - OS_MEMORY_MB)*1024*1024);
const int TOTAL_FRAMES = (USER_MEMORY_BYTES / PAGE_SIZE);
const int NFFMIN = 1000; // Minimum number of free frames to maintain
// Add these function prototypes


#define VALID_MASK 0x8000   // Bit 15 (MSB) - Valid bit
#define REFERENCE_MASK 0x4000  // Bit 14 - Reference bit
#define FRAME_MASK 0x3FFF   // Bits 0-13 - Frame number

typedef uint16_t pte_t;      // Page table entry
typedef uint16_t history_t;  // History for LRU

// FFLIST structure - Free Frame List entry
typedef struct {
    int frame;      // Frame number
    int owner_pid;  // Last owner (-1 if none)
    int owner_page; // Last owner's page (-1 if none)
} FFEntry;

// Process structure
typedef struct {
    int pid;               // Process ID
    int s;                 // Size of array
    int m;                 // Number of searches
    int current_search;    // Current search index
    int *search_keys;      // Array of search keys
    pte_t page_table[2048]; // Page table
    history_t history[2048]; // Page history for LRU
} Process;

// Statistics
typedef struct {
    long accesses;    // Page accesses
    long faults;      // Page faults
    long replacements; // Page replacements
    long attempt1;    // Replacements via attempt 1
    long attempt2;    // Replacements via attempt 2
    long attempt3;    // Replacements via attempt 3
    long attempt4;    // Replacements via attempt 4
    // Add these tracking variables
    long attempt1_prev;
    long attempt2_prev; 
    long attempt3_prev;
    long attempt4_prev;
} Stats;
typedef struct {
    bool replaced;
    int victim_page;
    int victim_frame;
    int victim_history;
    int attempt_used;
    int frame_used;
} ReplacementInfo;


// Queue structure for ready queue
typedef struct QueueNode {
    Process *proc;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *head, *tail;
    int count;
} Queue;

int find_victim_page(Process *p);
int find_free_frame(Process *p, int page, Stats *stats);
// Global variables
Process **procs;
FFEntry *fflist;
int nff; // Number of free frames
int n, m;
Stats *proc_stats; // Per-process statistics
Stats total_stats; // Global statistics
int min_active_processes = INT_MAX;
int current_frame_index = 0;  // Added: Index for ascending frame allocation

// Adds process to queue
void enqueue(Queue *q, Process *proc) {
    QueueNode *node = malloc(sizeof(QueueNode));
    node->proc = proc;
    node->next = NULL;
    
    if (q->tail)
        q->tail->next = node;
    else
        q->head = node;
    
    q->tail = node;
    q->count++;
}

// Removes process from queue
Process *dequeue(Queue *q) {
    if (q->head == NULL) return NULL;
    
    QueueNode *node = q->head;
    Process *proc = node->proc;
    q->head = node->next;
    
    if (q->head == NULL) 
        q->tail = NULL;
    
    free(node);
    q->count--;
    return proc;
}


ReplacementInfo load_page(Process *p, int page_index, Stats *stats) {
    ReplacementInfo info = {false, -1, -1, -1, -1, -1};
    
    // If we have free frames above minimum, use one
    if (nff > NFFMIN) {
        int frame = fflist[current_frame_index].frame;
        fflist[current_frame_index] = fflist[nff-1];
        nff--;
        
        current_frame_index = (current_frame_index + 1) % nff;
        
        p->page_table[page_index] = VALID_MASK | REFERENCE_MASK | frame;
        p->history[page_index] = 0xFFFF;
        
        info.frame_used = frame;
        return info;
    }
    
    // Need to do page replacement
    int victim_page = find_victim_page(p);
    if (victim_page == -1) {
        info.replaced = false;
        return info;
    }
    
    // Record replacement info
    info.replaced = true;
    info.victim_page = victim_page;
    info.victim_frame = p->page_table[victim_page] & FRAME_MASK;
    info.victim_history = p->history[victim_page];
    
    // Get victim frame
    int victim_frame = p->page_table[victim_page] & FRAME_MASK;
    
    // Invalidate victim page
    p->page_table[victim_page] &= ~VALID_MASK;
    
    // Add victim frame to FFLIST
    fflist[nff].frame = victim_frame;
    fflist[nff].owner_pid = p->pid;
    fflist[nff].owner_page = victim_page;
    nff++;
    
    // Find a free frame from FFLIST according to the attempts
    int frame = find_free_frame(p, page_index, stats);
    info.frame_used = frame;
    
    // Store which attempt was used (captured in find_free_frame)
    if (stats->attempt1 > 0 && total_stats.attempt1 > 0) info.attempt_used = 1;
    else if (stats->attempt2 > 0 && total_stats.attempt2 > 0) info.attempt_used = 2;
    else if (stats->attempt3 > 0 && total_stats.attempt3 > 0) info.attempt_used = 3;
    else info.attempt_used = 4;
    
    // Assign frame to page
    p->page_table[page_index] = VALID_MASK | REFERENCE_MASK | frame;
    p->history[page_index] = 0xFFFF;
    
    stats->replacements++;
    total_stats.replacements++;
    
    return info;
}

// Initialize frame list
void init_frame_list() {
    fflist = malloc(TOTAL_FRAMES * sizeof(FFEntry));
    rep(i, TOTAL_FRAMES) {
        fflist[i].frame = i;
        fflist[i].owner_pid = -1;
        fflist[i].owner_page = -1;
    }
    nff = TOTAL_FRAMES;
    current_frame_index = 0; // Start from beginning of list
}

// Load essential page for process
bool load_essential_page(Process *p, int page_index) {
    if (nff == 0) return false;
    
    // Get a free frame from current index
    int frame = fflist[current_frame_index].frame;
    
    // Move last frame to this position
    fflist[current_frame_index] = fflist[nff-1]; 
    nff--;
    
    // Increment current index
    current_frame_index = (current_frame_index + 1) % nff;
    
    // Assign frame to page
    p->page_table[page_index] = VALID_MASK | frame;
    p->history[page_index] = 0xFFFF; // Essential pages have maximum history
    
    return true;
}

// Update page histories after a binary search
void update_histories(Process *p) {
    rep(i, 2048) {
        if (p->page_table[i] & VALID_MASK) {
            // Shift history right and insert reference bit at MSB
            p->history[i] = (p->history[i] >> 1) | 
                           ((p->page_table[i] & REFERENCE_MASK) ? 0x8000 : 0);
            
            // Clear reference bit
            p->page_table[i] &= ~REFERENCE_MASK;
        }
    }
}

// Find victim page in process (page with minimum history)
int find_victim_page(Process *p) {
    int victim = -1;
    unsigned min_history = 0xFFFF;
    
    // Skip essential pages (0-9)
    repa(i,10,2048)  {
        if ((p->page_table[i] & VALID_MASK) && p->history[i] < min_history) {
            min_history = p->history[i];
            victim = i;
        }
    }
    
    return victim;
}

// Find free frame from FFLIST for page of process 
// Returns frame number or -1 if no frame found
int find_free_frame(Process *p, int page, Stats *stats) {
    // Attempt 1: Find frame with last owner = p->pid and page = page
    rep(i,nff) {
        if (fflist[i].owner_pid == p->pid && fflist[i].owner_page == page) {
            int frame = fflist[i].frame;
            fflist[i] = fflist[nff-1]; // Replace with last element
            nff--;
            stats->attempt1++;
            total_stats.attempt1++;
            return frame;
        }
    }
    
    // Attempt 2: Find frame with no owner
    rep(i,nff) {
        if (fflist[i].owner_pid == -1) {
            int frame = fflist[i].frame;
            fflist[i] = fflist[nff-1]; // Replace with last element
            nff--;
            stats->attempt2++;
            total_stats.attempt2++;
            return frame;
        }
    }
    
    // Attempt 3: Find frame whose last owner was p
    rep(i,nff) {
        if (fflist[i].owner_pid == p->pid) {
            int frame = fflist[i].frame;
            fflist[i] = fflist[nff-1]; // Replace with last element
            nff--;
            stats->attempt3++;
            total_stats.attempt3++;
            return frame;
        }
    }
    
    // Attempt 4: Get frame from current position
    int frame = fflist[current_frame_index].frame;
    fflist[current_frame_index] = fflist[nff-1]; // Replace with last element
    nff--;
    
    // Update current index for next allocation
    current_frame_index = (current_frame_index + 1) % nff;
    
    stats->attempt4++;
    total_stats.attempt4++;
    return frame;
}

// Free all frames used by a process
void free_process_frames(Process *p) {
    rep(i,2048) {
        if (p->page_table[i] & VALID_MASK) {
            int frame = p->page_table[i] & FRAME_MASK;
            fflist[nff].frame = frame;
            fflist[nff].owner_pid = -1; // No owner
            fflist[nff].owner_page = -1;
            nff++;
            p->page_table[i] = 0;
        }
    }
}

bool simulate_binary_search(Process *p, Stats *stats) {
    int key = p->search_keys[p->current_search];
    int left = 0;
    int right = p->s - 1;

 #ifdef VERBOSE
    printf("+++ Process %d: Search %d\n", p->pid, p->current_search + 1);
 #endif

    while (left < right) {
        int mid = (left + right) / 2;
        stats->accesses++;
        total_stats.accesses++;

        int page_index = 10 + (mid / INTS_PER_PAGE);

        if ((p->page_table[page_index] & VALID_MASK) == 0) {
            // Page fault
            stats->faults++;
            total_stats.faults++;

            ReplacementInfo info = load_page(p, page_index, stats);
            
            if (info.frame_used == -1) {
                return false; // Load failed
            }

 #ifdef VERBOSE
            if (info.replaced) {
                // This was a page replacement operation
                printf("    Fault on Page %4d: To replace Page %d at Frame %d [history = %d]\n", 
                       page_index, info.victim_page, info.victim_frame, info.victim_history);
                       
                // Print attempt information
                if (info.attempt_used == 1) {
                    printf("        Attempt 1: Free frame %4d owned by process %d page %d found\n", 
                           info.frame_used, p->pid, page_index);
                } else if (info.attempt_used == 2) {
                    printf("        Attempt 2: Free frame %4d owned by no process found\n", 
                           info.frame_used);
                } else if (info.attempt_used == 3) {
                    printf("        Attempt 3: Free frame %4d owned by process %d found\n", 
                           info.frame_used, p->pid);
                } else {
                    printf("        Attempt 4: Free frame %4d randomly selected\n", 
                           info.frame_used);
                }
            } else {
                // This was a regular free frame allocation
                printf("    Fault on Page %4d: Free frame %4d found\n", 
                       page_index, info.frame_used);
            }
 #endif
        } else {
            // Set reference bit
            p->page_table[page_index] |= REFERENCE_MASK;
        }

        if (key <= mid) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }

    // Update page histories after search
    update_histories(p);

    return true;
}

int main() {
    srand(time(NULL));
    
    // Read input
    FILE *fp = fopen("search.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open search.txt\n");
        return 1;
    }
    
    fscanf(fp, "%d %d", &n, &m);
    
    // Initialize statistics
    proc_stats = malloc(n * sizeof(Stats));
    memset(proc_stats, 0, n * sizeof(Stats));
    memset(&total_stats, 0, sizeof(Stats));
    
    // Allocate processes
    procs = malloc(n * sizeof(Process *));
    
    rep(i,n){
        procs[i] = malloc(sizeof(Process));
        procs[i]->pid = i;
        procs[i]->current_search = 0;
        procs[i]->m = m;
        
        fscanf(fp, "%d", &procs[i]->s);
        procs[i]->search_keys = malloc(m * sizeof(int));
        
        rep(j,m) {
            fscanf(fp, "%d", &procs[i]->search_keys[j]);
        }
        
        // Initialize page table and history
        memset(procs[i]->page_table, 0, 2048 * sizeof(pte_t));
        memset(procs[i]->history, 0, 2048 * sizeof(history_t));
    }
    
    fclose(fp);
    
    // Initialize frame list
    init_frame_list();
    
    // Initialize ready queue
    Queue ready_queue = {NULL, NULL, 0};
    
    printf("+++ Simulation data read from file\n");
    printf("+++ Kernel data initialized\n");
    
    // Load essential pages for all processes
    rep(i, n) {
        rep(j,10) {
            if (!load_essential_page(procs[i], j)) {
                fprintf(stderr, "Error: not enough memory for essential pages\n");
                exit(1);
            }
        }
        enqueue(&ready_queue, procs[i]);
    }
    
    // Simulation loop
    int active_processes = n;
    
    while (active_processes > 0) {
        Process *p = dequeue(&ready_queue);
        
        // Skip finished processes
        if (p->current_search >= p->m) continue;
        
        // Run one binary search
        bool success = simulate_binary_search(p, &proc_stats[p->pid]);
        
        if (success) {
            p->current_search++;
            
            if (p->current_search < p->m) {
                // More searches to do
                enqueue(&ready_queue, p);
            } else {
                // Process completed all searches
                free_process_frames(p);
                active_processes--;
            }
        } else {
            // Should not happen with proper implementation
            fprintf(stderr, "Error: search failed for process %d\n", p->pid);
            enqueue(&ready_queue, p);
        }
        
        // Track minimum active processes
        if (ready_queue.count < min_active_processes && ready_queue.count > 0) {
            min_active_processes = ready_queue.count;
        }
    }
    
    // Print statistics
    printf("\n+++ Page access summary\n\n");
    printf(" PID Accesses Faults Replacements Attempts\n");
    
    rep(i,n){
        Stats *s = &proc_stats[i];
        printf(" %3lld %7lu %5lu (%5.2f%%) %5lu (%5.2f%%) %3lu + %3lu + %3lu + %3lu (%5.2f%% + %5.2f%% + %5.2f%% + %5.2f%%)\n",
        i, s->accesses, s->faults, 100.0 * s->faults / s->accesses,
            s->replacements, 100.0 * s->replacements / s->accesses,
            s->attempt1, s->attempt2, s->attempt3, s->attempt4,
            s->replacements > 0 ? 100.0 * s->attempt1 / s->replacements : 0.0,
            s->replacements > 0 ? 100.0 * s->attempt2 / s->replacements : 0.0,
            s->replacements > 0 ? 100.0 * s->attempt3 / s->replacements : 0.0,
            s->replacements > 0 ? 100.0 * s->attempt4 / s->replacements : 0.0);
    }
    
    // Print total statistics
    printf(" Total %7lu %5lu (%5.2f%%) %5lu (%5.2f%%) %3lu + %3lu + %3lu + %3lu (%5.2f%% + %5.2f%% + %5.2f%% + %5.2f%%)\n",
        total_stats.accesses, total_stats.faults, 100.0 * total_stats.faults / total_stats.accesses,
        total_stats.replacements, 100.0 * total_stats.replacements / total_stats.accesses,
        total_stats.attempt1, total_stats.attempt2, total_stats.attempt3, total_stats.attempt4,
        100.0 * total_stats.attempt1 / total_stats.replacements,
        100.0 * total_stats.attempt2 / total_stats.replacements,
        100.0 * total_stats.attempt3 / total_stats.replacements,
        100.0 * total_stats.attempt4 / total_stats.replacements);
    
    // Cleanup
    for (int i = 0; i < n; i++) {
        free(procs[i]->search_keys);
        free(procs[i]);
    }
    free(procs);
    free(fflist);
    free(proc_stats);
    
    return 0;
}