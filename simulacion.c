#include <stdio.h>
#include <stdlib.h>

//
// ======== ENUMS Y ESTRUCTURAS BÁSICAS ========
//

// Tipo de ráfaga: CPU o I/O
typedef enum { CPU_BURST, IO_BURST } BurstType;

// Estado del proceso
typedef enum { NEW, READY, RUNNING, WAITING, TERMINATED } ProcessState;

// Representa una ráfaga
typedef struct {
    BurstType type;
    int duration;
    int remaining;
} Burst;

// Representa un proceso
typedef struct {
    int pid;
    int arrival_time;
    Burst *bursts;
    int burst_count;
    int current_burst;
    ProcessState state;

    // Estadísticas
    int start_time;
    int finish_time;
    int total_wait_time;
} Process;

// Nodo de la cola
typedef struct Node {
    Process *process;
    struct Node *next;
} Node;

// Cola (ready o waiting)
typedef struct {
    Node *front;
    Node *rear;
} Queue;

// Estado general del simulador
typedef struct {
    int clock;
    Queue ready_queue;
    Queue waiting_queue;
    Process *running;
    Process *processes;
    int process_count;
} Simulator;


//
// ======== FUNCIONES DE COLA ========
//

void init_queue(Queue *q) {
    q->front = q->rear = NULL;
}

int is_empty(Queue *q) {
    return q->front == NULL;
}

void enqueue(Queue *q, Process *p) {
    Node *node = (Node *)malloc(sizeof(Node));
    node->process = p;
    node->next = NULL;

    if (q->rear == NULL)
        q->front = q->rear = node;
    else {
        q->rear->next = node;
        q->rear = node;
    }
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


//
// ======== FUNCIONES DEL SIMULADOR ========
//

// Mueve procesos que llegan al sistema
void move_arrivals(Simulator *sim) {
    for (int i = 0; i < sim->process_count; i++) {
        Process *p = &sim->processes[i];
        if (p->arrival_time == sim->clock && p->state == NEW) {
            p->state = READY;
            enqueue(&sim->ready_queue, p);
            printf("[t=%d] Proceso %d llega y entra a READY\n", sim->clock, p->pid);
        }
    }
}

// Actualiza los procesos en la cola de espera (I/O)
void update_waiting(Simulator *sim) {
    int size = 0; // tamaño aproximado de la cola
    Node *n = sim->waiting_queue.front;
    while (n) {
        size++;
        n = n->next;
    }

    for (int i = 0; i < size; i++) {
        Process *p = dequeue(&sim->waiting_queue);
        Burst *b = &p->bursts[p->current_burst];

        b->remaining--;
        if (b->remaining == 0) {
            p->current_burst++;
            if (p->current_burst < p->burst_count) {
                p->state = READY;
                enqueue(&sim->ready_queue, p);
                printf("[t=%d] Proceso %d termina I/O y pasa a READY\n", sim->clock, p->pid);
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

// Actualiza el proceso en ejecución
void update_running(Simulator *sim) {
    if (sim->running == NULL) return;

    Process *p = sim->running;
    Burst *b = &p->bursts[p->current_burst];
    b->remaining--;

    if (b->remaining == 0) {
        p->current_burst++;

        // Verificar si hay más ráfagas
        if (p->current_burst < p->burst_count) {
            // Si la siguiente es I/O, pasa a waiting
            if (p->bursts[p->current_burst].type == IO_BURST) {
                p->state = WAITING;
                enqueue(&sim->waiting_queue, p);
                printf("[t=%d] Proceso %d pasa a WAITING\n", sim->clock, p->pid);
            } else {
                p->state = READY;
                enqueue(&sim->ready_queue, p);
            }
        } else {
            p->state = TERMINATED;
            p->finish_time = sim->clock;
            printf("[t=%d] Proceso %d finaliza\n", sim->clock, p->pid);
        }
        sim->running = NULL; // CPU queda libre
    }
}

// Despacha un nuevo proceso al CPU (FCFS por ahora)
void dispatch_if_idle(Simulator *sim) {
    if (sim->running != NULL) return;
    if (is_empty(&sim->ready_queue)) return;

    Process *p = dequeue(&sim->ready_queue);
    p->state = RUNNING;
    sim->running = p;

    if (p->start_time == -1)
        p->start_time = sim->clock;

    printf("[t=%d] Proceso %d entra a CPU\n", sim->clock, p->pid);
}

// Comprueba si todos terminaron
int all_terminated(Simulator *sim) {
    for (int i = 0; i < sim->process_count; i++) {
        if (sim->processes[i].state != TERMINATED)
            return 0;
    }
    return 1;
}

// Bucle principal
void run_simulation(Simulator *sim) {
    while (!all_terminated(sim)) {
        move_arrivals(sim);
        update_waiting(sim);
        update_running(sim);
        dispatch_if_idle(sim);

        sim->clock++;
    }
}


//
// ======== MAIN DE EJEMPLO ========
//

int main() {
    // Proceso 1: CPU (3) -> IO (2) -> CPU (3)
    Burst p1_bursts[] = {
        {CPU_BURST, 3, 3},
        {IO_BURST, 2, 2},
        {CPU_BURST, 3, 3}
    };

    // Proceso 2: CPU (4)
    Burst p2_bursts[] = {
        {CPU_BURST, 4, 4}
    };

    Process processes[] = {
        {1, 0, p1_bursts, 3, 0, NEW, -1, 0, 0},
        {2, 2, p2_bursts, 1, 0, NEW, -1, 0, 0}
    };

    Simulator sim = {0};
    sim.processes = processes;
    sim.process_count = 2;
    init_queue(&sim.ready_queue);
    init_queue(&sim.waiting_queue);
    sim.running = NULL;
    sim.clock = 0;

    run_simulation(&sim);

    printf("\n=== Resultados ===\n");
    for (int i = 0; i < sim.process_count; i++) {
        Process *p = &sim.processes[i];
        printf("P%d: start=%d, finish=%d, turnaround=%d\n",
               p->pid, p->start_time, p->finish_time,
               p->finish_time - p->arrival_time);
    }

    return 0;
}
