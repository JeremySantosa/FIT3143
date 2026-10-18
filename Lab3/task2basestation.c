#include "common.h"

#include <limits.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * comments + function headers were created using GPT 5.6, to speed up coding implementation and readability for documentation for other readers. 
 * used to also "prettify" code, to make all indent and spaces consistent, allowing better readability to user. 

*/

/*
 * MPI protocol:
 * Messages are transmitted as raw struct bytes. Both sides must use the
 * same common.h, ABI, and architecture. For heterogeneous systems, use
 * MPI derived datatypes instead.
 */

typedef struct {
    int has_report;
    double received_at;
    NodeReport report;
} ReportHistory;

/* Check whether a node rank belongs to the charging-node grid. */
static int valid_rank(int rank, int nodes)
{
    return rank >= 0 && rank < nodes;
}

/* Compute squared Euclidean distance between two grid nodes. */
static int squared_distance(int a, int b, int cols)
{
    int dr = a / cols - b / cols;
    int dc = a % cols - b % cols;

    return dr * dr + dc * dc;
}

/* List the immediate top, bottom, left, and right neighbours. */
static int direct_neighbours(int rank, int rows, int cols, int out[4])
{
    int r = rank / cols;
    int c = rank % cols;
    int count = 0;

    if (r > 0) {
        out[count++] = rank - cols;
    }
    if (r + 1 < rows) {
        out[count++] = rank + cols;
    }
    if (c > 0) {
        out[count++] = rank - 1;
    }
    if (c + 1 < cols) {
        out[count++] = rank + 1;
    }

    return count;
}

/* Determine whether a node has reported within the specified time window. */
static int recent(const ReportHistory *history, double now, double window)
{
    return history->has_report &&
           now >= history->received_at &&
           now - history->received_at <= window;
}

/*
 * Task 2: Two-hop neighbourhood selection.
 *
 * - Task 1 has already checked the reporting node's immediate neighbours.
 * - Search the next layer (neighbours of those neighbours).
 * - A recent congestion report means that a node is treated as congested.
 * - Positive port availability is considered when supplied by reports.
 * - No recent congestion report makes a node a candidate, not a verified vacancy.
 * - Return every candidate tied at the shortest squared Euclidean distance.
 */
static int choose_candidates(
    const ReportHistory *history,
    int rows,
    int cols,
    const NodeReport *report,
    double now,
    double window,
    BaseResponse *response)
{
    int nodes = rows * cols;
    int origin = report->node_rank;
    int immediate[4];
    int immediate_count = direct_neighbours(origin, rows, cols, immediate);
    int all_immediate_recent = immediate_count > 0;

    for (int i = 0; i < immediate_count; ++i) {
        if (!recent(&history[immediate[i]], now, window)) {
            all_immediate_recent = 0;
        }
    }

    response->reporting_node = origin;
    response->available_count = 0;

    /* All immediate neighbours reported recently: no nearby options. */
    if (all_immediate_recent) {
        return 0;
    }

    int *seen = calloc((size_t)nodes, sizeof(int));
    if (!seen) {
        return -1;
    }

    int min_distance = INT_MAX;

    for (int j = 0; j < immediate_count; ++j) {
        int second[4];
        int second_count = direct_neighbours(
            immediate[j], rows, cols, second
        );

        for (int k = 0; k < second_count; ++k) {
            int candidate = second[k];

            if (candidate == origin || seen[candidate]) {
                continue;
            }
            seen[candidate] = 1;

            if (recent(&history[candidate], now, window)) {
                continue;
            }

            /*
             * Exclude candidates explicitly reported as full by a recent
             * neighbour observation. Otherwise, absence of a recent report
             * leaves the node as a possible candidate under this model.
             */
            int known_availability = -1;

            for (int src = 0; src < nodes; ++src) {
                if (!recent(&history[src], now, window)) {
                    continue;
                }

                const NodeReport *source_report = &history[src].report;

                for (int p = 0;
                     p < source_report->neighbour_count && p < MAX_NEIGHBOURS;
                     ++p) {
                    if (source_report->neighbour_ranks[p] == candidate) {
                        known_availability =
                            source_report->neighbour_availability[p];
                    }
                }
            }

            if (known_availability == 0) {
                continue;
            }

            int distance = squared_distance(origin, candidate, cols);

            if (distance < min_distance) {
                min_distance = distance;
                response->available_count = 0;
            }

            if (distance == min_distance &&
                response->available_count < MAX_RESPONSE_NODES) {
                response->available_nodes[response->available_count++] = candidate;
            }
        }
    }

    free(seen);
    return response->available_count;
}

/* Main base station process. */
int run_base_station(
    int rows,
    int cols,
    double simulation_seconds,
    double report_window_seconds,
    const char *log_path)
{
    int rank = -1;
    int size = 0;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rows <= 0 || cols <= 0 ||
        (long long)rows * cols != size - 1 ||
        rank != size - 1 ||
        simulation_seconds <= 0 || report_window_seconds <= 0) {
        fprintf(stderr, "Base station: invalid grid, rank or time settings.\n");
        return 1;
    }

    int nodes = rows * cols;
    ReportHistory *history = calloc((size_t)nodes, sizeof(*history));

    if (!history) {
        fprintf(stderr, "Base station: allocation failure\n");
        return 1;
    }

    FILE *log = fopen(log_path ? log_path : "base_station.log", "w");
    if (!log) {
        perror("Opening base station log");
        free(history);
        return 1;
    }

    double start = MPI_Wtime();
    unsigned long alerts = 0;
    unsigned long inbound = 0;
    unsigned long outbound = 0;

    fprintf(log,
            "simulation_seconds=%.3f window_seconds=%.3f grid=%dx%d\n",
            simulation_seconds, report_window_seconds, rows, cols);
    fflush(log);

    printf("Base station rank %d running for %.1f s, grid %dx%d\n",
           rank, simulation_seconds, rows, cols);

    /* Receive and process congestion reports during the simulation. */
    while (MPI_Wtime() - start < simulation_seconds) {
        int flag = 0;
        MPI_Status status;

        MPI_Iprobe(MPI_ANY_SOURCE, REPORT_TAG, MPI_COMM_WORLD, &flag, &status);
        if (!flag) {
            continue;
        }

        NodeReport report = {0};
        MPI_Recv(&report, (int)sizeof(report), MPI_BYTE,
                 status.MPI_SOURCE, REPORT_TAG, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
        inbound++;

        if (!valid_rank(status.MPI_SOURCE, nodes)) {
            continue;
        }

        /* Use the actual MPI sender rank, rather than the payload value. */
        report.node_rank = status.MPI_SOURCE;

        if (report.neighbour_count < 0 ||
            report.neighbour_count > MAX_NEIGHBOURS) {
            fprintf(log, "bad_neighbour_count rank=%d\n", report.node_rank);
            continue;
        }

        double now = MPI_Wtime();

        history[report.node_rank].has_report = 1;
        history[report.node_rank].received_at = now;
        history[report.node_rank].report = report;
        alerts++;

        BaseResponse reply = {0};
        int count = choose_candidates(
            history, rows, cols, &report, now,
            report_window_seconds, &reply
        );

        if (count < 0) {
            fclose(log);
            free(history);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }

        /* Log incident details and selected candidate nodes. */
        fprintf(log,
                "t=%.6f sender=%d xy=(%d,%d) ports=%d "
                "incident_timestamp=%.0f neighbours=%d "
                "neighbour_messages=%d neighbour_bytes=%d candidates=%d",
                now - start, report.node_rank, report.x, report.y,
                report.available_ports, report.timestamp,
                report.neighbour_count, report.message_count,
                report.message_size, reply.available_count);

        for (int i = 0; i < reply.available_count; ++i) {
            fprintf(log, " %d", reply.available_nodes[i]);
        }

        fprintf(log, "\n");
        fflush(log);

        MPI_Send(&reply, (int)sizeof(reply), MPI_BYTE,
                 report.node_rank, RESPONSE_TAG, MPI_COMM_WORLD);
        outbound++;
    }

    /*
     * Ask every charging node to stop.
     * Task 1 must continue accepting TERMINATE_TAG during shutdown.
     */
    int termination = 1;

    for (int node = 0; node < nodes; ++node) {
        MPI_Send(&termination, 1, MPI_INT,
                 node, TERMINATE_TAG, MPI_COMM_WORLD);
        outbound++;
    }

    fprintf(log,
            "summary elapsed_seconds=%.6f alerts=%lu inbound_reports=%lu "
            "outbound_messages=%lu total_base_messages=%lu\n",
            MPI_Wtime() - start, alerts, inbound, outbound,
            inbound + outbound);

    fflush(log);
    fclose(log);
    free(history);

    return 0;
}
