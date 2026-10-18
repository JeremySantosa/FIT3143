# FIT3143 Lab 3 — Task 2: Base Station

This README explains how to integrate `task2basestation.c` with Task 1. Task 2 is written in **C with Open MPI** and uses the shared `common.h` file.

## Files to share

- `task2basestation.c` — base station implementation (does **not** contain `main()`).
- `common.h` — message tags and shared `NodeReport` / `BaseResponse` structures. **Both Task 1 and Task 2 must use the same version.**

## How to call Task 2

Task 2 exports this function:

```c
int run_base_station(int rows, int cols,
                     double simulation_seconds,
                     double report_window_seconds,
                     const char *log_path);
```

In the integrated `main.c`, initialise MPI once, assign the **last MPI rank** to the base station, and call the function only on that rank. The other ranks should execute Task 1's charging-node function:

```c
#include <mpi.h>
#include "common.h"

// Declared here because this project does not use a base_station.h header.
int run_base_station(int rows, int cols, double simulation_seconds,
                     double report_window_seconds, const char *log_path);

// Replace with your teammate's actual function declaration:
void run_charging_node(int rows, int cols);

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int rows = 3, cols = 3;
    if (size != rows * cols + 1) {
        if (rank == 0) {
            // 9 EV nodes + 1 base station = 10 processes.
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int result = 0;
    if (rank == rows * cols) {
        result = run_base_station(rows, cols,
                                  DEFAULT_SIMULATION_TIME,
                                  DEFAULT_TIME_WINDOW,
                                  "base_station.log");
    } else {
        run_charging_node(rows, cols); // Adapt to Task 1's actual signature.
    }

    MPI_Finalize();
    return result;
}
```

The example is an **integration template**, not a standalone program: `run_charging_node` must be replaced with the actual Task 1 function and signature.

## Compile and run

Once your teammate has provided `main.c` and `charging_node.c`:

```bash
mpicc -std=c11 -Wall -Wextra main.c charging_node.c task2basestation.c -o simulation
mpirun -np 10 ./simulation
```

Use `-np (rows * cols + 1)`. For a 3 × 3 grid, ranks **0–8** are EV nodes and rank **9** is the base station. The base station assumes these ranks map to row-major grid coordinates: `(rank / cols, rank % cols)`. Task 1's Cartesian rank mapping must agree.

## What the base station does

1. Listens for congestion reports with `MPI_Iprobe()` / `MPI_Recv()` using `REPORT_TAG`.
2. Stores the latest report and receipt time for each node.
3. Checks previous reports within `report_window_seconds` and evaluates two-hop candidate nodes, returning all candidates tied for the shortest squared Euclidean distance.
4. Sends a `BaseResponse` to the reporting node using `RESPONSE_TAG`.
5. Logs incidents, candidates, alerts and message counts to the specified log file.
6. After `simulation_seconds`, sends `TERMINATE_TAG` to every charging node and frees its own resources.

**Availability caveat:** A node without a recent congestion report is treated as a *candidate*, not as a verified free charging station. This implements a particular interpretation of the assignment's report-history rule; test it against Task 1 scenarios.

## MPI interface — important for Task 1

| Direction | Tag | Payload |
|---|---|---|
| EV node → base station | `REPORT_TAG` | `NodeReport` (`sizeof(NodeReport)` bytes, `MPI_BYTE`) |
| Base station → EV node | `RESPONSE_TAG` | `BaseResponse` (`sizeof(BaseResponse)` bytes, `MPI_BYTE`) |
| Base station → EV node | `TERMINATE_TAG` | One `int`, `MPI_INT` |

The current implementation transmits **raw C struct bytes**. All processes must use the same `common.h`, compatible struct layout/ABI and architecture. The charging-node code must continue to handle termination, including while waiting for messages, to avoid hanging on shutdown. Both sides must agree on MPI message tags and the send/receive order.

## Before submitting

Run integration tests using different grid sizes and report windows, verify multiple congestion alerts and equal-distance candidates, inspect `base_station.log`, and confirm a clean MPI shutdown. The full Task 1 + Task 2 integration **has not yet been verified** by this README.
