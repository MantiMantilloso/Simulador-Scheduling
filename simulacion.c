#include <stdio.h>
#include <stdlib.h>

typedef enum { CPU_BURST, IO_BURST } BurstType;
typedef enum { NEW, READY, RUNNING, WAITING, TERMINATED } ProcessState;

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

typedef struct {
    int clock;
    int quantum;
    int quantum_remaining;
    Queue ready_queue;
    Queue waiting_queue;
    Process *running;
    Process *processes;
    int process_count;
} Simulator;

// =================== Cola ===================
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

// =================== Simulación ===================

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
                enqueue(&sim->ready_queue, p);
                printf("[t=%d] Proceso %d termina I/O y pasa a READY\n", sim->clock, p->pid);
            } else {
                p->state = TERMINATED;
                p->finish_time = sim->clock;
                printf("[t=%d] Proceso %d finaliza\n", sim->clock, p->pid);
            }
        } else enqueue(&sim->waiting_queue, p);
    }
}

void update_running(Simulator *sim) {
    if (sim->running == NULL) return;
    Process *p = sim->running;
    Burst *b = &p->bursts[p->current_burst];

    b->remaining--;
    sim->quantum_remaining--;

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
                enqueue(&sim->ready_queue, p);
            }
        } else {
            p->state = TERMINATED;
            p->finish_time = sim->clock;
            printf("[t=%d] Proceso %d finaliza\n", sim->clock, p->pid);
        }
    }
    else if (sim->quantum_remaining == 0) {
        // Quantum agotado, preempción
        p->state = READY;
        enqueue(&sim->ready_queue, p);
        sim->running = NULL;
        printf("[t=%d] Proceso %d preemptado (quantum agotado)\n", sim->clock, p->pid);
    }
}

void dispatch_if_idle(Simulator *sim) {
    if (sim->running != NULL) return;
    if (is_empty(&sim->ready_queue)) return;

    Process *p = dequeue(&sim->ready_queue);
    p->state = RUNNING;
    sim->running = p;
    sim->quantum_remaining = sim->quantum;

    if (p->start_time == -1)
        p->start_time = sim->clock;

    printf("[t=%d] Proceso %d entra a CPU (quantum=%d)\n",
           sim->clock, p->pid, sim->quantum_remaining);
}

int all_terminated(Simulator *sim) {
    for (int i = 0; i < sim->process_count; i++)
        if (sim->processes[i].state != TERMINATED)
            return 0;
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

// =================== MAIN ===================

int main() {
    Burst p1_bursts[] = {
        {CPU_BURST, 5, 5},
        {IO_BURST, 3, 3},
        {CPU_BURST, 4, 4}
    };
    Burst p2_bursts[] = {
        {CPU_BURST, 6, 6}
    };
    Process processes[] = {
        {1, 0, p1_bursts, 3, 0, NEW, -1, 0, 0},
        {2, 2, p2_bursts, 1, 0, NEW, -1, 0, 0}
    };

    Simulator sim = {0};
    sim.quantum = 3;   // 🔹 quantum configurable
    sim.processes = processes;
    sim.process_count = 2;
    init_queue(&sim.ready_queue);
    init_queue(&sim.waiting_queue);

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
