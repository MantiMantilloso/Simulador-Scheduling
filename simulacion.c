#include <stdio.h>
#include <stdlib.h>

typedef enum { CPU_BURST, IO_BURST } BurstType;
typedef enum { NEW, READY, RUNNING, WAITING, TERMINATED } ProcessState;
typedef enum { FCFS, ROUND_ROBIN, SJF } SchedulerPolicy;

typedef struct {
    BurstType type;
    int duration;
    int remaining;
} Burst;

typedef struct {
    int pid;
    int arrival_time;
    Burst *bursts;
    int burst_count;
    int current_burst;
    ProcessState state;

    int start_time;
    int finish_time;
    int total_wait_time;
} Process;

typedef struct Node {
    Process *process;
    struct Node *next;
} Node;

typedef struct {
    Node *front;
    Node *rear;
} Queue;

// =================== Priority Queue para SJF ===================
typedef struct PQNode {
    Process *process;
    struct PQNode *next;
    int priority; // Duración del siguiente burst de CPU
} PQNode;

typedef struct {
    PQNode *front;
} PriorityQueue;

// =================== Simulador ===================
typedef struct {
    int clock;
    int quantum;
    int quantum_remaining;
    Queue ready_queue;        // Para FCFS y Round Robin
    PriorityQueue sjf_queue;  // Para SJF
    Queue waiting_queue;
    Process *running;
    Process *processes;
    SchedulerPolicy policy;
    int process_count;
} Simulator;

// =================== Funciones de Cola Regular ===================
void init_queue(Queue *q) { q->front = q->rear = NULL; }
int is_empty(Queue *q) { return q->front == NULL; }

void enqueue(Queue *q, Process *p) {
    Node *n = malloc(sizeof(Node));
    n->process = p;
    n->next = NULL;
    if (q->rear == NULL) q->front = q->rear = n;
    else { q->rear->next = n; q->rear = n; }
}

Process *dequeue(Queue *q) {
    if (is_empty(q)) return NULL;
    Node *temp = q->front;
    Process *p = temp->process;
    q->front = q->front->next;
    if (q->front == NULL) q->rear = NULL;
    free(temp);
    return p;
}

// =================== Funciones de Priority Queue para SJF ===================
void init_priority_queue(PriorityQueue *pq) { pq->front = NULL; }

int is_priority_empty(PriorityQueue *pq) { return pq->front == NULL; }

void priority_enqueue(PriorityQueue *pq, Process *p) {
    // Calcular la duración del siguiente burst de CPU
    int cpu_duration = 0;
    for (int i = p->current_burst; i < p->burst_count; i++) {
        if (p->bursts[i].type == CPU_BURST) {
            cpu_duration = p->bursts[i].duration;
            break;
        }
    }
    
    PQNode *new_node = malloc(sizeof(PQNode));
    new_node->process = p;
    new_node->priority = cpu_duration;
    new_node->next = NULL;

    // Insertar en orden de prioridad (menor duración primero)
    if (is_priority_empty(pq) || cpu_duration < pq->front->priority) {
        new_node->next = pq->front;
        pq->front = new_node;
    } else {
        PQNode *current = pq->front;
        while (current->next != NULL && current->next->priority <= cpu_duration) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
}

Process *priority_dequeue(PriorityQueue *pq) {
    if (is_priority_empty(pq)) return NULL;
    PQNode *temp = pq->front;
    Process *p = temp->process;
    pq->front = pq->front->next;
    free(temp);
    return p;
}

// =================== Funciones de Simulación ===================
void move_arrivals(Simulator *sim) {
    for (int i = 0; i < sim->process_count; i++) {
        Process *p = &sim->processes[i];
        if (p->arrival_time == sim->clock && p->state == NEW) {
            p->state = READY;
            if (sim->policy == SJF) {
                priority_enqueue(&sim->sjf_queue, p);
                printf("[t=%d] Proceso %d llega y entra a READY (próximo CPU: %d)\n", 
                       sim->clock, p->pid, p->bursts[p->current_burst].duration);
            } else {
                enqueue(&sim->ready_queue, p);
                printf("[t=%d] Proceso %d llega y entra a READY\n", sim->clock, p->pid);
            }
        }
    }
}

void update_waiting(Simulator *sim) {
    int size = 0;
    for (Node *n = sim->waiting_queue.front; n; n = n->next) size++;

    for (int i = 0; i < size; i++) {
        Process *p = dequeue(&sim->waiting_queue);
        Burst *b = &p->bursts[p->current_burst];
        b->remaining--;

        if (b->remaining == 0) {
            p->current_burst++;
            if (p->current_burst < p->burst_count) {
                p->state = READY;
                if (sim->policy == SJF) {
                    priority_enqueue(&sim->sjf_queue, p);
                    printf("[t=%d] Proceso %d termina I/O y pasa a READY\n", sim->clock, p->pid);
                } else {
                    enqueue(&sim->ready_queue, p);
                    printf("[t=%d] Proceso %d termina I/O y pasa a READY\n", sim->clock, p->pid);
                }
            } else {
                p->state = TERMINATED;
                p->finish_time = sim->clock;
                printf("[t=%d] Proceso %d finaliza\n", sim->clock, p->pid);
            }
        } else {
            enqueue(&sim->waiting_queue, p);
        }
    }
}

void update_running(Simulator *sim) {
    if (sim->running == NULL) return;
    Process *p = sim->running;
    Burst *b = &p->bursts[p->current_burst];

    b->remaining--;
    if (sim->policy == ROUND_ROBIN) {
        sim->quantum_remaining--;
    }

    if (b->remaining == 0) {
        p->current_burst++;
        sim->running = NULL;

        if (p->current_burst < p->burst_count) {
            if (p->bursts[p->current_burst].type == IO_BURST) {
                p->state = WAITING;
                enqueue(&sim->waiting_queue, p);
                printf("[t=%d] Proceso %d pasa a WAITING\n", sim->clock, p->pid);
            } else {
                p->state = READY;
                if (sim->policy == SJF) {
                    priority_enqueue(&sim->sjf_queue, p);
                } else {
                    enqueue(&sim->ready_queue, p);
                }
            }
        } else {
            p->state = TERMINATED;
            p->finish_time = sim->clock;
            printf("[t=%d] Proceso %d finaliza\n", sim->clock, p->pid);
        }
    }
    else if (sim->policy == ROUND_ROBIN && sim->quantum_remaining == 0) {
        // Quantum agotado, preempción solo en Round Robin
        p->state = READY;
        enqueue(&sim->ready_queue, p);
        sim->running = NULL;
        printf("[t=%d] Proceso %d preemptado (quantum agotado)\n", sim->clock, p->pid);
    }
}

void dispatch_if_idle(Simulator *sim) {
    if (sim->running != NULL) return;
    
    Process *p = NULL;
    
    if (sim->policy == SJF) {
        if (!is_priority_empty(&sim->sjf_queue)) {
            p = priority_dequeue(&sim->sjf_queue);
        }
    } else {
        if (!is_empty(&sim->ready_queue)) {
            p = dequeue(&sim->ready_queue);
        }
    }
    
    if (p == NULL) return;
    
    p->state = RUNNING;
    sim->running = p;
    
    if (sim->policy == ROUND_ROBIN) {
        sim->quantum_remaining = sim->quantum;
    }

    if (p->start_time == -1) {
        p->start_time = sim->clock;
    }

    if (sim->policy == ROUND_ROBIN) {
        printf("[t=%d] Proceso %d entra a CPU (quantum=%d)\n",
               sim->clock, p->pid, sim->quantum_remaining);
    } else {
        printf("[t=%d] Proceso %d entra a CPU\n", sim->clock, p->pid);
    }
}

int all_terminated(Simulator *sim) {
    for (int i = 0; i < sim->process_count; i++) {
        if (sim->processes[i].state != TERMINATED) {
            return 0;
        }
    }
    return 1;
}

void run_simulation(Simulator *sim) {
    while (!all_terminated(sim)) {
        move_arrivals(sim);
        update_waiting(sim);
        update_running(sim);
        dispatch_if_idle(sim);
        sim->clock++;
    }
}

// =================== MÉTRICAS ===================
void print_statistics(Simulator *sim) {
    int total_turnaround = 0;
    int total_response = 0;
    int completed = 0;

    printf("\n=== Estadísticas ===\n");
    for (int i = 0; i < sim->process_count; i++) {
        Process *p = &sim->processes[i];
        if (p->state == TERMINATED) {
            int turnaround = p->finish_time - p->arrival_time;
            int response = p->start_time - p->arrival_time;

            total_turnaround += turnaround;
            total_response += response;
            completed++;

            printf("P%d: arrival=%d, start=%d, finish=%d, turnaround=%d, response=%d\n",
                   p->pid, p->arrival_time, p->start_time, p->finish_time,
                   turnaround, response);
        }
    }

    if (completed > 0) {
        double avg_turnaround = (double) total_turnaround / completed;
        double avg_response = (double) total_response / completed;
        double throughput = (double) completed / sim->clock;

        printf("\n=== Promedios ===\n");
        printf("Avg Turnaround: %.2f\n", avg_turnaround);
        printf("Avg Response: %.2f\n", avg_response);
        printf("Throughput: %.3f procesos/unidad de tiempo\n", throughput);
    }
}

// =================== MAIN ===================
int main() {
    Burst p1_bursts[] = {
        {CPU_BURST, 2, 2}, {IO_BURST, 3, 3},
        {CPU_BURST, 3, 3}, {IO_BURST, 2, 2},
        {CPU_BURST, 1, 1}, {IO_BURST, 4, 4},
        {CPU_BURST, 2, 2}
    };
    
    Burst p2_bursts[] = {
        {CPU_BURST, 1, 1}, {IO_BURST, 2, 2},
        {CPU_BURST, 2, 2}, {IO_BURST, 1, 1},
        {CPU_BURST, 1, 1}, {IO_BURST, 3, 3},
        {CPU_BURST, 3, 3}
    };
    
    Burst p3_bursts[] = {
        {CPU_BURST, 3, 3}, {IO_BURST, 2, 2},
        {CPU_BURST, 2, 2}, {IO_BURST, 3, 3},
        {CPU_BURST, 1, 1}, {IO_BURST, 1, 1},
        {CPU_BURST, 2, 2}
    };
    
    Burst p4_bursts[] = {
        {CPU_BURST, 1, 1}, {IO_BURST, 4, 4},
        {CPU_BURST, 2, 2}, {IO_BURST, 2, 2},
        {CPU_BURST, 1, 1}, {IO_BURST, 3, 3},
        {CPU_BURST, 3, 3}
    };
    
    Burst p5_bursts[] = {
        {CPU_BURST, 2, 2}, {IO_BURST, 1, 1},
        {CPU_BURST, 3, 3}, {IO_BURST, 2, 2},
        {CPU_BURST, 1, 1}, {IO_BURST, 2, 2},
        {CPU_BURST, 2, 2}
    };
    
    // Procesos CPU Bound (5 procesos) - CPU bursts largos, pocos I/O bursts
    Burst p6_bursts[] = {
        {CPU_BURST, 8, 8}, {IO_BURST, 2, 2}, {CPU_BURST, 6, 6}
    };
    
    Burst p7_bursts[] = {
        {CPU_BURST, 10, 10}, {IO_BURST, 1, 1}, {CPU_BURST, 5, 5}
    };
    
    Burst p8_bursts[] = {
        {CPU_BURST, 12, 12}
    };
    
    Burst p9_bursts[] = {
        {CPU_BURST, 7, 7}, {IO_BURST, 3, 3}, {CPU_BURST, 8, 8}
    };
    
    Burst p10_bursts[] = {
        {CPU_BURST, 9, 9}, {IO_BURST, 2, 2}, {CPU_BURST, 6, 6}
    };
    
    Process processes[] = {
        // I/O Bound processes
        {1, 0, p1_bursts, 7, 0, NEW, -1, 0, 0},   // Tiempo total CPU: 8
        {2, 1, p2_bursts, 7, 0, NEW, -1, 0, 0},   // Tiempo total CPU: 7  
        {3, 2, p3_bursts, 7, 0, NEW, -1, 0, 0},   // Tiempo total CPU: 8
        {4, 3, p4_bursts, 7, 0, NEW, -1, 0, 0},   // Tiempo total CPU: 7
        {5, 4, p5_bursts, 7, 0, NEW, -1, 0, 0},   // Tiempo total CPU: 9
        
        // CPU Bound processes  
        {6, 0, p6_bursts, 3, 0, NEW, -1, 0, 0},   // Tiempo total CPU: 14
        {7, 1, p7_bursts, 3, 0, NEW, -1, 0, 0},   // Tiempo total CPU: 15
        {8, 2, p8_bursts, 1, 0, NEW, -1, 0, 0},   // Tiempo total CPU: 12
        {9, 3, p9_bursts, 3, 0, NEW, -1, 0, 0},   // Tiempo total CPU: 15
        {10, 4, p10_bursts, 3, 0, NEW, -1, 0, 0}  // Tiempo total CPU: 15
    };
    
    printf("Seleccione política de planificación:\n");
    printf("1. FCFS\n");
    printf("2. Round Robin\n");
    printf("3. SJF (Shortest Job First)\n> ");
    
    int choice;
    scanf("%d", &choice);
    
    Simulator sim = {0};
    
    switch (choice) {
        case 1:
            sim.policy = FCFS;
            sim.quantum = 1000; // Muy grande para simular FCFS
            printf("Seleccionado FCFS\n");
            break;
        case 2:
            sim.policy = ROUND_ROBIN;
            printf("Ingrese el quantum: ");
            scanf("%d", &sim.quantum);
            printf("Seleccionado Round Robin (quantum=%d)\n", sim.quantum);
            break;
        case 3:
            sim.policy = SJF;
            sim.quantum = 1000; // No se usa en SJF, pero lo inicializamos
            printf("Seleccionado SJF (Shortest Job First)\n");
            break;
        default:
            printf("Opción inválida. Usando FCFS por defecto.\n");
            sim.policy = FCFS;
            sim.quantum = 1000;
            break;
    }

    sim.processes = processes;
    sim.process_count = 10;
    init_queue(&sim.ready_queue);
    init_priority_queue(&sim.sjf_queue);
    init_queue(&sim.waiting_queue);

    run_simulation(&sim);
    print_statistics(&sim);

    return 0;
}