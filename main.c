#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

// ─────────────────────────────────────────────
//  CONSTANTS
// ─────────────────────────────────────────────
#define MAX_PROCESSES   10
#define MAX_THREADS     10
#define MAX_RESOURCES   5
#define MAX_NAME        32

// ─────────────────────────────────────────────
//  PROCESS MANAGEMENT
// ─────────────────────────────────────────────
typedef enum { READY, RUNNING, WAITING, TERMINATED } ProcessState;

typedef struct {
    int         pid;
    char        name[MAX_NAME];
    ProcessState state;
    int         priority;       // 1 (low) – 5 (high)
    int         burst_time;     // simulated CPU burst (seconds)
    int         remaining_time;
} Process;

static Process  processes[MAX_PROCESSES];
static int      process_count = 0;
static int      next_pid      = 1;

const char *state_str(ProcessState s) {
    switch (s) {
        case READY:      return "READY";
        case RUNNING:    return "RUNNING";
        case WAITING:    return "WAITING";
        case TERMINATED: return "TERMINATED";
        default:         return "UNKNOWN";
    }
}

void create_process(const char *name, int priority, int burst) {
    if (process_count >= MAX_PROCESSES) {
        printf("  [!] Process table full.\n");
        return;
    }
    Process *p     = &processes[process_count++];
    p->pid         = next_pid++;
    strncpy(p->name, name, MAX_NAME - 1);
    p->state        = READY;
    p->priority     = priority;
    p->burst_time   = burst;
    p->remaining_time = burst;
    printf("  [+] Process created  PID=%d  Name=%-12s  Priority=%d  Burst=%ds\n",
           p->pid, p->name, p->priority, p->burst_time);
}

void list_processes(void) {
    if (process_count == 0) { printf("  No processes.\n"); return; }
    printf("  %-6s %-14s %-12s %-10s %-8s\n",
           "PID", "Name", "State", "Priority", "Burst");
    printf("  %s\n", "──────────────────────────────────────────────────");
    for (int i = 0; i < process_count; i++) {
        Process *p = &processes[i];
        printf("  %-6d %-14s %-12s %-10d %-8d\n",
               p->pid, p->name, state_str(p->state), p->priority, p->burst_time);
    }
}

// Priority (non-preemptive) scheduler simulation
void run_scheduler(void) {
    printf("\n  [Scheduler] Running Priority Scheduling (non-preemptive)...\n\n");
    // collect READY processes
    int order[MAX_PROCESSES], cnt = 0;
    for (int i = 0; i < process_count; i++)
        if (processes[i].state == READY)
            order[cnt++] = i;

    if (cnt == 0) { printf("  No READY processes to schedule.\n"); return; }

    // sort descending priority (insertion sort)
    for (int i = 1; i < cnt; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && processes[order[j]].priority < processes[key].priority) {
            order[j+1] = order[j]; j--;
        }
        order[j+1] = key;
    }

    int time = 0;
    for (int i = 0; i < cnt; i++) {
        Process *p = &processes[order[i]];
        p->state = RUNNING;
        printf("  t=%-4d  Running  PID=%d (%s)  burst=%ds\n",
               time, p->pid, p->name, p->burst_time);
        time += p->burst_time;
        p->state = TERMINATED;
        printf("  t=%-4d  Done     PID=%d (%s)\n\n", time, p->pid, p->name);
    }
    printf("  [Scheduler] All processes completed. Total time = %ds\n", time);
}

void kill_process(int pid) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].pid == pid) {
            processes[i].state = TERMINATED;
            printf("  [x] PID %d (%s) terminated.\n", pid, processes[i].name);
            return;
        }
    }
    printf("  [!] PID %d not found.\n", pid);
}

// ─────────────────────────────────────────────
//  THREAD MANAGEMENT
// ─────────────────────────────────────────────
typedef struct {
    int     tid;
    char    name[MAX_NAME];
    int     active;
    pthread_t handle;
    int     sleep_sec;
} ThreadEntry;

static ThreadEntry  threads[MAX_THREADS];
static int          thread_count = 0;
static int          next_tid     = 1;
static pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

void *thread_worker(void *arg) {
    ThreadEntry *t = (ThreadEntry *)arg;
    printf("  [Thread %d] '%s' started.\n", t->tid, t->name);
    sleep(t->sleep_sec);
    printf("  [Thread %d] '%s' finished after %ds.\n", t->tid, t->name, t->sleep_sec);
    pthread_mutex_lock(&thread_mutex);
    t->active = 0;
    pthread_mutex_unlock(&thread_mutex);
    return NULL;
}

void spawn_thread(const char *name, int duration) {
    if (thread_count >= MAX_THREADS) { printf("  [!] Thread table full.\n"); return; }
    ThreadEntry *t = &threads[thread_count++];
    t->tid       = next_tid++;
    strncpy(t->name, name, MAX_NAME - 1);
    t->active    = 1;
    t->sleep_sec = duration;
    pthread_create(&t->handle, NULL, thread_worker, t);
    printf("  [+] Thread spawned  TID=%d  Name=%s  Duration=%ds\n",
           t->tid, t->name, duration);
}

void list_threads(void) {
    if (thread_count == 0) { printf("  No threads.\n"); return; }
    printf("  %-6s %-16s %-8s\n", "TID", "Name", "Status");
    printf("  %s\n", "──────────────────────────────────");
    for (int i = 0; i < thread_count; i++) {
        ThreadEntry *t = &threads[i];
        printf("  %-6d %-16s %-8s\n",
               t->tid, t->name, t->active ? "ALIVE" : "DONE");
    }
}

void join_all_threads(void) {
    printf("  Joining all threads...\n");
    for (int i = 0; i < thread_count; i++)
        if (threads[i].active)
            pthread_join(threads[i].handle, NULL);
    printf("  All threads joined.\n");
}

// ─────────────────────────────────────────────
//  BANKER'S ALGORITHM  (Deadlock Prevention)
// ─────────────────────────────────────────────
static int  num_procs = 0;
static int  num_res   = 0;
static int  available[MAX_RESOURCES];
static int  maximum[MAX_PROCESSES][MAX_RESOURCES];
static int  allocation[MAX_PROCESSES][MAX_RESOURCES];
static int  need[MAX_PROCESSES][MAX_RESOURCES];
static char bnames[MAX_PROCESSES][MAX_NAME];

void bankers_init(void) {
    printf("\n  ── Banker's Algorithm Setup ──\n");
    printf("  Number of processes (max %d): ", MAX_PROCESSES);
    scanf("%d", &num_procs);
    printf("  Number of resource types (max %d): ", MAX_RESOURCES);
    scanf("%d", &num_res);

    printf("  Available resources (space-separated): ");
    for (int j = 0; j < num_res; j++) scanf("%d", &available[j]);

    for (int i = 0; i < num_procs; i++) {
        printf("  Process %d name: ", i);
        scanf("%s", bnames[i]);
        printf("  Max claim for %s: ", bnames[i]);
        for (int j = 0; j < num_res; j++) scanf("%d", &maximum[i][j]);
        printf("  Current allocation for %s: ", bnames[i]);
        for (int j = 0; j < num_res; j++) {
            scanf("%d", &allocation[i][j]);
            need[i][j] = maximum[i][j] - allocation[i][j];
        }
    }
    printf("  [+] Banker's state initialised.\n");
}

int bankers_safe(int *safe_seq) {
    int work[MAX_RESOURCES], finish[MAX_PROCESSES];
    memcpy(work, available, sizeof(int)*num_res);
    memset(finish, 0, sizeof(finish));

    int count = 0;
    while (count < num_procs) {
        int found = 0;
        for (int i = 0; i < num_procs; i++) {
            if (finish[i]) continue;
            int ok = 1;
            for (int j = 0; j < num_res; j++)
                if (need[i][j] > work[j]) { ok = 0; break; }
            if (ok) {
                for (int j = 0; j < num_res; j++)
                    work[j] += allocation[i][j];
                finish[i]         = 1;
                safe_seq[count++] = i;
                found             = 1;
            }
        }
        if (!found) return 0; // unsafe
    }
    return 1;
}

void bankers_check(void) {
    if (num_procs == 0) { printf("  [!] Run 'banker-init' first.\n"); return; }
    int seq[MAX_PROCESSES];
    printf("\n  Checking safety...\n");
    if (bankers_safe(seq)) {
        printf("  [✓] SAFE STATE  –  Safe sequence: ");
        for (int i = 0; i < num_procs; i++)
            printf("%s%s", bnames[seq[i]], i < num_procs-1 ? " → " : "\n");
    } else {
        printf("  [✗] UNSAFE STATE – System may deadlock!\n");
    }
}

void bankers_request(void) {
    if (num_procs == 0) { printf("  [!] Run 'banker-init' first.\n"); return; }
    char pname[MAX_NAME];
    int  req[MAX_RESOURCES];
    printf("  Process name: "); scanf("%s", pname);

    int pi = -1;
    for (int i = 0; i < num_procs; i++)
        if (strcmp(bnames[i], pname) == 0) { pi = i; break; }
    if (pi < 0) { printf("  [!] Process not found.\n"); return; }

    printf("  Request vector (%d values): ", num_res);
    for (int j = 0; j < num_res; j++) scanf("%d", &req[j]);

    // Step 1: request <= need?
    for (int j = 0; j < num_res; j++)
        if (req[j] > need[pi][j]) {
            printf("  [!] Request exceeds maximum claim. Denied.\n"); return;
        }
    // Step 2: request <= available?
    for (int j = 0; j < num_res; j++)
        if (req[j] > available[j]) {
            printf("  [!] Resources unavailable. Process must wait.\n"); return;
        }
    // Step 3: tentative allocation
    for (int j = 0; j < num_res; j++) {
        available[j]      -= req[j];
        allocation[pi][j] += req[j];
        need[pi][j]       -= req[j];
    }
    int seq[MAX_PROCESSES];
    if (bankers_safe(seq)) {
        printf("  [✓] Request GRANTED. New safe sequence: ");
        for (int i = 0; i < num_procs; i++)
            printf("%s%s", bnames[seq[i]], i < num_procs-1 ? " → " : "\n");
    } else {
        // rollback
        for (int j = 0; j < num_res; j++) {
            available[j]      += req[j];
            allocation[pi][j] -= req[j];
            need[pi][j]       += req[j];
        }
        printf("  [✗] Request DENIED – would lead to unsafe state.\n");
    }
}

// ─────────────────────────────────────────────
//  DEADLOCK DETECTION  (Resource Allocation Graph)
// ─────────────────────────────────────────────
#define MAX_RAG_NODES  (MAX_PROCESSES + MAX_RESOURCES)

static int  rag_procs = 0, rag_res = 0;
static char rag_pnames[MAX_PROCESSES][MAX_NAME];
static char rag_rnames[MAX_RESOURCES][MAX_NAME];
static int  rag_alloc[MAX_PROCESSES][MAX_RESOURCES];  // resource i → process j
static int  rag_req  [MAX_PROCESSES][MAX_RESOURCES];  // process i requests resource j

void rag_init(void) {
    printf("\n  ── Resource Allocation Graph Setup ──\n");
    printf("  Number of processes: "); scanf("%d", &rag_procs);
    for (int i = 0; i < rag_procs; i++) {
        printf("  Process %d name: ", i); scanf("%s", rag_pnames[i]);
    }
    printf("  Number of resources: "); scanf("%d", &rag_res);
    for (int j = 0; j < rag_res; j++) {
        printf("  Resource %d name: ", j); scanf("%s", rag_rnames[j]);
    }
    memset(rag_alloc, 0, sizeof(rag_alloc));
    memset(rag_req,   0, sizeof(rag_req));

    // allocations
    printf("\n  Enter allocations (resource → process):\n");
    for (int j = 0; j < rag_res; j++) {
        printf("  How many instances of %s are allocated? ", rag_rnames[j]);
        int n; scanf("%d", &n);
        for (int k = 0; k < n; k++) {
            char pn[MAX_NAME]; printf("    Allocated to (process name): "); scanf("%s", pn);
            for (int i = 0; i < rag_procs; i++)
                if (strcmp(rag_pnames[i], pn) == 0) { rag_alloc[i][j]++; break; }
        }
    }
    // requests
    printf("\n  Enter requests (process → resource):\n");
    for (int i = 0; i < rag_procs; i++) {
        printf("  Does %s request any resource? (1=yes 0=no): ", rag_pnames[i]);
        int yn; scanf("%d", &yn);
        if (yn) {
            char rn[MAX_NAME]; printf("    Which resource? "); scanf("%s", rn);
            for (int j = 0; j < rag_res; j++)
                if (strcmp(rag_rnames[j], rn) == 0) { rag_req[i][j] = 1; break; }
        }
    }
    printf("  [+] RAG initialised.\n");
}

// Cycle detection via DFS on wait-for graph
int rag_dfs(int node, int visited[], int rec_stack[], int adj[][MAX_PROCESSES]) {
    visited[node]   = 1;
    rec_stack[node] = 1;
    for (int n = 0; n < rag_procs; n++) {
        if (!adj[node][n]) continue;
        if (!visited[n] && rag_dfs(n, visited, rec_stack, adj)) return 1;
        if (rec_stack[n]) return 1;
    }
    rec_stack[node] = 0;
    return 0;
}

void rag_detect(void) {
    if (rag_procs == 0) { printf("  [!] Run 'rag-init' first.\n"); return; }

    // Build wait-for graph: P_i waits for P_k if P_i requests R_j and P_k holds R_j
    int adj[MAX_PROCESSES][MAX_PROCESSES];
    memset(adj, 0, sizeof(adj));
    for (int i = 0; i < rag_procs; i++)
        for (int j = 0; j < rag_res; j++)
            if (rag_req[i][j])
                for (int k = 0; k < rag_procs; k++)
                    if (rag_alloc[k][j]) adj[i][k] = 1;

    printf("\n  Wait-for graph edges:\n");
    int any = 0;
    for (int i = 0; i < rag_procs; i++)
        for (int k = 0; k < rag_procs; k++)
            if (adj[i][k]) {
                printf("    %s → %s\n", rag_pnames[i], rag_pnames[k]);
                any = 1;
            }
    if (!any) printf("    (none)\n");

    int visited[MAX_PROCESSES]   = {0};
    int rec_stack[MAX_PROCESSES] = {0};
    int deadlock = 0;
    for (int i = 0; i < rag_procs; i++)
        if (!visited[i] && rag_dfs(i, visited, rec_stack, adj)) { deadlock = 1; break; }

    if (deadlock)
        printf("\n  [✗] DEADLOCK DETECTED – cycle exists in wait-for graph!\n");
    else
        printf("\n  [✓] No deadlock – no cycle detected.\n");
}

// ─────────────────────────────────────────────
//  HELP
// ─────────────────────────────────────────────
void print_help(void) {
    printf("\n");
    printf("  ╔══════════════════════════════════════════════════════╗\n");
    printf("  ║          OS Simulation – Command Reference           ║\n");
    printf("  ╠══════════════════════════════════════════════════════╣\n");
    printf("  ║  PROCESS MANAGEMENT                                  ║\n");
    printf("  ║    ps-create <name> <priority(1-5)> <burst(s)>       ║\n");
    printf("  ║    ps-list                                           ║\n");
    printf("  ║    ps-schedule   – run priority scheduler            ║\n");
    printf("  ║    ps-kill <pid>                                     ║\n");
    printf("  ╠══════════════════════════════════════════════════════╣\n");
    printf("  ║  THREAD MANAGEMENT                                   ║\n");
    printf("  ║    th-spawn <name> <duration(s)>                     ║\n");
    printf("  ║    th-list                                           ║\n");
    printf("  ║    th-join    – wait for all threads to finish       ║\n");
    printf("  ╠══════════════════════════════════════════════════════╣\n");
    printf("  ║  DEADLOCK PREVENTION (Banker's Algorithm)            ║\n");
    printf("  ║    banker-init    – set up resource/process state    ║\n");
    printf("  ║    banker-check   – check if system is in safe state ║\n");
    printf("  ║    banker-request – simulate a resource request      ║\n");
    printf("  ╠══════════════════════════════════════════════════════╣\n");
    printf("  ║  DEADLOCK DETECTION (Resource Allocation Graph)      ║\n");
    printf("  ║    rag-init    – set up RAG                          ║\n");
    printf("  ║    rag-detect  – detect deadlock via cycle detection ║\n");
    printf("  ╠══════════════════════════════════════════════════════╣\n");
    printf("  ║    help   clear   exit                               ║\n");
    printf("  ╚══════════════════════════════════════════════════════╝\n\n");
}

// ─────────────────────────────────────────────
//  MAIN  CLI  LOOP
// ─────────────────────────────────────────────
int main(void) {
    char line[256], cmd[64], a1[64], a2[64], a3[64];

    printf("\n");
    printf("  ╔══════════════════════════════════════════════════════╗\n");
    printf("  ║   OS Process & Thread Management Simulator  v1.0    ║\n");
    printf("  ║   Topics: Scheduling · Threads · Deadlock           ║\n");
    printf("  ╚══════════════════════════════════════════════════════╝\n");
    printf("  Type 'help' to see available commands.\n\n");

    while (1) {
        printf("  os-sim> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        if (!line[0]) continue;

        memset(cmd,0,sizeof(cmd));
        memset(a1,0,sizeof(a1));
        memset(a2,0,sizeof(a2));
        memset(a3,0,sizeof(a3));
        sscanf(line, "%63s %63s %63s %63s", cmd, a1, a2, a3);

        if      (!strcmp(cmd, "exit"))           { join_all_threads(); printf("  Goodbye.\n\n"); break; }
        else if (!strcmp(cmd, "clear"))          { system("clear"); }
        else if (!strcmp(cmd, "help"))           { print_help(); }
        // process
        else if (!strcmp(cmd, "ps-create"))      { create_process(a1, atoi(a2), atoi(a3)); }
        else if (!strcmp(cmd, "ps-list"))        { list_processes(); }
        else if (!strcmp(cmd, "ps-schedule"))    { run_scheduler(); }
        else if (!strcmp(cmd, "ps-kill"))        { kill_process(atoi(a1)); }
        // thread
        else if (!strcmp(cmd, "th-spawn"))       { spawn_thread(a1, atoi(a2)); }
        else if (!strcmp(cmd, "th-list"))        { list_threads(); }
        else if (!strcmp(cmd, "th-join"))        { join_all_threads(); }
        // banker
        else if (!strcmp(cmd, "banker-init"))    { bankers_init(); }
        else if (!strcmp(cmd, "banker-check"))   { bankers_check(); }
        else if (!strcmp(cmd, "banker-request")) { bankers_request(); }
        // rag
        else if (!strcmp(cmd, "rag-init"))       { rag_init(); }
        else if (!strcmp(cmd, "rag-detect"))     { rag_detect(); }
        else { printf("  Unknown command: '%s'. Type 'help'.\n", cmd); }

        printf("\n");
    }
    return 0;
}