# OS Process & Thread Management Simulator

A CLI-based C program that simulates core OS concepts:
- **Process creation & priority scheduling**
- **POSIX thread management**
- **Deadlock prevention** via Banker's Algorithm
- **Deadlock detection** via Resource Allocation Graph (RAG) cycle detection

---

## Build

```bash
make
```

Requires GCC and POSIX threads (`-lpthread`).

---

## Run

```bash
./os_sim
```

---

## Command Reference

### Process Management
| Command | Description |
|---------|-------------|
| `ps-create <name> <priority 1-5> <burst_sec>` | Create a new process |
| `ps-list` | List all processes and their states |
| `ps-schedule` | Run non-preemptive priority scheduler |
| `ps-kill <pid>` | Terminate a process |

**Example:**
```
ps-create Alpha 5 3
ps-create Beta  2 5
ps-create Gamma 4 2
ps-schedule
```

---

### Thread Management
| Command | Description |
|---------|-------------|
| `th-spawn <name> <duration_sec>` | Spawn a real POSIX thread |
| `th-list` | List threads and their live/done status |
| `th-join` | Block until all threads complete |

**Example:**
```
th-spawn Worker1 2
th-spawn Worker2 4
th-list
th-join
```

---

### Deadlock Prevention – Banker's Algorithm
| Command | Description |
|---------|-------------|
| `banker-init` | Interactive setup: processes, resources, allocations |
| `banker-check` | Check if current state is safe; prints safe sequence |
| `banker-request` | Simulate a resource request; grant only if state stays safe |

**Example session:**
```
banker-init
  Number of processes: 3
  Number of resource types: 3
  Available resources: 3 3 2
  Process 0 name: P0   Max: 7 5 3   Allocation: 0 1 0
  Process 1 name: P1   Max: 3 2 2   Allocation: 2 0 0
  Process 2 name: P2   Max: 9 0 2   Allocation: 3 0 2

banker-check
  → Safe sequence: P1 → P0 → P2

banker-request
  Process name: P1
  Request vector: 1 0 2
  → Request GRANTED
```

---

### Deadlock Detection – Resource Allocation Graph
| Command | Description |
|---------|-------------|
| `rag-init` | Interactive setup of processes, resources, allocations, requests |
| `rag-detect` | Build wait-for graph and run DFS cycle detection |

**Example (deadlock scenario):**
```
rag-init
  Processes: P0, P1
  Resources: R0, R1
  R0 allocated to P0 | R1 allocated to P1
  P0 requests R1     | P1 requests R0

rag-detect
  → DEADLOCK DETECTED
```

---

## Other Commands
- `help` — show command reference
- `clear` — clear screen
- `exit` — join threads and quit
