#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
// =================== Generadores de Workloads ===================

// Crea un burst con duración e inicializa remaining
Burst make_burst(BurstType type, int duration) {
    Burst b;
    b.type = type;
    b.duration = duration;
    b.remaining = duration;
    return b;
}

// Genera 'count' bursts para un proceso, favoreciendo I/O o CPU según is_io_bound
void generate_bursts(Burst *b, int count, int is_io_bound) {
    for (int i = 0; i < count; i++) {
        int choose = rand() % 100;
        BurstType t;
        if (is_io_bound) {
            // Alta probabilidad de I/O
            t = (choose < 75) ? IO_BURST : CPU_BURST; // ~75% I/O
        } else {
            // Alta probabilidad de CPU
            t = (choose < 75) ? CPU_BURST : IO_BURST; // ~75% CPU
        }

        int dur;
        if (t == CPU_BURST) {
            if (is_io_bound) dur = (rand() % 3) + 1;    // 1-3 (corto)
            else dur = (rand() % 7) + 6;                // 6-12 (largo)
        } else {
            if (is_io_bound) dur = (rand() % 5) + 4;    // 4-8 (I/O más largo)
            else dur = (rand() % 3) + 1;                // 1-3 (I/O corto)
        }

        b[i] = make_burst(t, dur);
    }
}

// Crea un workload de 'n' procesos con proporción io_fraction (ej: 0.9 para 90% I/O)
Process *create_workload(int n, double io_fraction, int bursts_per_proc) {
    Process *procs = malloc(sizeof(Process) * n);
    if (!procs) {
        fprintf(stderr, "Error al reservar memoria para procesos\n");
        exit(1);
    }

    int num_io = (int)(n * io_fraction + 0.5);

    for (int i = 0; i < n; i++) {
        int is_io = (i < num_io);
        procs[i].pid = i + 1;
        procs[i].arrival_time = i / 10; // agrupar 10 procesos por unidad de tiempo
        procs[i].burst_count = bursts_per_proc;
        procs[i].current_burst = 0;
        procs[i].state = NEW;
        procs[i].start_time = -1;
        procs[i].finish_time = 0;
        procs[i].total_wait_time = 0;
        procs[i].bursts = malloc(sizeof(Burst) * bursts_per_proc);
        if (!procs[i].bursts) {
            fprintf(stderr, "Error al reservar memoria para bursts del proceso %d\n", i+1);
            exit(1);
        }
        generate_bursts(procs[i].bursts, bursts_per_proc, is_io);
    }

    return procs;
}

// =================== MAIN ===================
int main() {
    srand((unsigned) time(NULL));

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

    // Selección del workload (100 procesos)
    printf("\nSeleccione workload:\n");
    printf("1. 90%% I/O-bound, 10%% CPU-bound\n");
    printf("2. 50%% I/O-bound, 50%% CPU-bound\n");
    printf("3. 10%% I/O-bound, 90%% CPU-bound\n> ");

    int wchoice;
    scanf("%d", &wchoice);

    Process *dynamic_processes = NULL;
    switch (wchoice) {
        case 1:
            dynamic_processes = create_workload(100, 0.90, 10);
            printf("Seleccionado workload 90%% I/O-bound\n");
            break;
        case 2:
            dynamic_processes = create_workload(100, 0.50, 10);
            printf("Seleccionado workload 50/50\n");
            break;
        case 3:
            dynamic_processes = create_workload(100, 0.10, 10);
            printf("Seleccionado workload 90%% CPU-bound\n");
            break;
        default:
            printf("Opción inválida. Usando workload 50/50 por defecto.\n");
            dynamic_processes = create_workload(100, 0.50, 10);
            break;
    }

    sim.processes = dynamic_processes;
    sim.process_count = 100;
    init_queue(&sim.ready_queue);
    init_priority_queue(&sim.sjf_queue);
    init_queue(&sim.waiting_queue);

    run_simulation(&sim);
    print_statistics(&sim);

    return 0;
}