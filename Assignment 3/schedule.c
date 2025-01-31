#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#ifdef VERBOSE
#define VERBOSE_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define VERBOSE_PRINT(fmt, ...) ((void)0)
#endif

#define MAX_BURSTS 100
#define MAX_PROCESSES 1000

typedef enum {
    NEW, READY, RUNNING, WAITING, TERMINATED
} ProcessState;

typedef enum {
    PROCESS_ARRIVAL, CPU_BURST_END, IO_BURST_END, CPU_TIMEOUT
} EventType;

typedef struct {
    int id;
    int arrival_time;
    int cpu_bursts[MAX_BURSTS];
    int io_bursts[MAX_BURSTS];
    int num_bursts;
    ProcessState state;
    int current_burst;
    int remaining_time;
    int wait_time;
    int last_ready_time;
    int completion_time;
    int total_runtime;
} Process;

typedef struct {
    int time;
    int process_idx;
    EventType type;
} Event;

Process processes[MAX_PROCESSES];
Event event_heap[MAX_PROCESSES * MAX_BURSTS];
int heap_size = 0;
int ready_queue[MAX_PROCESSES];
int ready_front = 0, ready_rear = 0;
int current_process = -1;
int current_time = 0;
int n_processes;

void swap_events(Event *a, Event *b) {
    Event temp = *a;
    *a = *b;
    *b = temp;
}

void push_event(Event evt) {
    int i = heap_size++;
    event_heap[i] = evt;
    
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (event_heap[parent].time > event_heap[i].time ||
            (event_heap[parent].time == event_heap[i].time &&
             event_heap[parent].process_idx > event_heap[i].process_idx)) {
            swap_events(&event_heap[parent], &event_heap[i]);
            i = parent;
        } else break;
    }
}

Event pop_event() {
    Event min_evt = event_heap[0];
    event_heap[0] = event_heap[--heap_size];
    
    int i = 0;
    while (1) {
        int min_idx = i;
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        
        if (left < heap_size && 
            (event_heap[left].time < event_heap[min_idx].time ||
             (event_heap[left].time == event_heap[min_idx].time &&
              event_heap[left].process_idx < event_heap[min_idx].process_idx)))
            min_idx = left;
            
        if (right < heap_size && 
            (event_heap[right].time < event_heap[min_idx].time ||
             (event_heap[right].time == event_heap[min_idx].time &&
              event_heap[right].process_idx < event_heap[min_idx].process_idx)))
            min_idx = right;
            
        if (min_idx != i) {
            swap_events(&event_heap[i], &event_heap[min_idx]);
            i = min_idx;
        } else break;
    }
    
    return min_evt;
}

void enqueue_ready(int process_idx) {
    ready_queue[ready_rear++] = process_idx;
    
    Process *p = &processes[process_idx];
    p->last_ready_time = current_time;
}

int dequeue_ready() {
    if (ready_front == ready_rear) return -1;
    return ready_queue[ready_front++];
}

void schedule_process(int quantum) {
    if (current_process != -1) return;
    
    int next_process = dequeue_ready();
    if (next_process == -1) return;
    
    current_process = next_process;
    Process *p = &processes[next_process];
    p->state = RUNNING;
    
    p->wait_time += current_time - p->last_ready_time;
    
    int remaining = p->remaining_time;
    int run_time = (quantum < remaining) ? quantum : remaining;
    
    Event evt = {
        .time = current_time + run_time,
        .process_idx = next_process,
        .type = (run_time == remaining) ? CPU_BURST_END : CPU_TIMEOUT
    };
    
    push_event(evt);
    
    VERBOSE_PRINT("%d : Process %d is scheduled to run for time %d\n", current_time, p->id, run_time);
}

int calculate_runtime(Process *p) {
    int runtime = 0;
    for (int i = 0; i < p->num_bursts; i++) {
        runtime += p->cpu_bursts[i];
        if (p->io_bursts[i] != -1)
            runtime += p->io_bursts[i];
    }
    return runtime;
}

void simulate_scheduler(int quantum) {
    heap_size = 0;
    ready_front = ready_rear = 0;
    current_process = -1;
    current_time = 0;
    
    VERBOSE_PRINT("0 : Starting\n");
    
    for (int i = 0; i < n_processes; i++) {
        Process *p = &processes[i];
        p->state = NEW;
        p->current_burst = 0;
        p->remaining_time = p->cpu_bursts[0];
        p->wait_time = 0;
        p->last_ready_time = 0;
        
        Event evt = {
            .time = p->arrival_time,
            .process_idx = i,
            .type = PROCESS_ARRIVAL
        };
        push_event(evt);
    }
    
    printf("\n**** %s ****\n", 
           quantum == INT_MAX ? "FCFS Scheduling" : 
           quantum == 10 ? "RR Scheduling with q = 10" :
           "RR Scheduling with q = 5");
    
    int total_idle_time = 0;
    
    while (heap_size > 0) {
        Event evt = pop_event();
        
        if (current_process == -1 && evt.time > current_time) {
            total_idle_time += evt.time - current_time;
            VERBOSE_PRINT("%d : CPU goes idle\n", current_time);
        }
            
        current_time = evt.time;
        Process *p = &processes[evt.process_idx];
        
        switch (evt.type) {
            case PROCESS_ARRIVAL:
                p->state = READY;
                enqueue_ready(evt.process_idx);
                VERBOSE_PRINT("%d : Process %d joins ready queue upon arrival\n", current_time, p->id);
                break;
                
            case CPU_BURST_END:
                current_process = -1;
                p->current_burst++;
                
                if (p->io_bursts[p->current_burst-1] == -1) {
                    p->state = TERMINATED;
                    p->completion_time = current_time;
                    int turnaround = p->completion_time - p->arrival_time;
                    printf("%d : Process %d exits. Turnaround time = %d (%d%%), Wait time = %d\n",
                           current_time, p->id, turnaround,
                           (turnaround * 100) / p->total_runtime,
                           p->wait_time);
                } else {
                    p->state = WAITING;
                    Event io_evt = {
                        .time = current_time + p->io_bursts[p->current_burst-1],
                        .process_idx = evt.process_idx,
                        .type = IO_BURST_END
                    };
                    push_event(io_evt);
                }
                break;
                
            case IO_BURST_END:
                p->state = READY;
                p->remaining_time = p->cpu_bursts[p->current_burst];
                enqueue_ready(evt.process_idx);
                VERBOSE_PRINT("%d : Process %d joins ready queue after IO completion\n", current_time, p->id);
                break;
                
            case CPU_TIMEOUT:
                current_process = -1;
                p->remaining_time -= quantum;
                p->state = READY;
                enqueue_ready(evt.process_idx);
                VERBOSE_PRINT("%d : Process %d joins ready queue after timeout\n", current_time, p->id);
                break;
        }
        
        schedule_process(quantum);
    }
    
    float total_wait = 0;
    for (int i = 0; i < n_processes; i++)
        total_wait += processes[i].wait_time;
        
    printf("Average wait time = %.2f\n", total_wait / n_processes);
    printf("Total turnaround time = %d\n", current_time);
    printf("CPU idle time = %d\n", total_idle_time);
    printf("CPU utilization = %.2f%%\n", 
           100.0 * (current_time - total_idle_time) / current_time);
}

int main() {
    FILE *fp = fopen("proc.txt", "r");
    if (!fp) {
        printf("Error: Cannot open proc.txt\n");
        return 1;
    }
    
    fscanf(fp, "%d", &n_processes);
    
    for (int i = 0; i < n_processes; i++) {
        Process *p = &processes[i];
        fscanf(fp, "%d %d", &p->id, &p->arrival_time);
        
        int j = 0;
        while (1) {
            fscanf(fp, "%d", &p->cpu_bursts[j]);
            fscanf(fp, "%d", &p->io_bursts[j]);
            if (p->io_bursts[j] == -1) {
                j++;
                break;
            }
            j++;
        }
        p->num_bursts = j;
        p->total_runtime = calculate_runtime(p);
    }
    
    fclose(fp);
    
    simulate_scheduler(INT_MAX);  // FCFS
    simulate_scheduler(10);       // RR with q=10
    simulate_scheduler(5);        // RR with q=5
    
    return 0;
}