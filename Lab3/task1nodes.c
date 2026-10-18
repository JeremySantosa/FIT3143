#include <stdio.h>
#include <mpi.h> // MPI header
#include <omp.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h> // usleep
#include "common.h"

#define K 6 // Ports per node

// task 2
int run_base_station(int rows, int cols, double simulation_seconds, double report_window_seconds, const char *log_path);

static FILE *timing_log = NULL; // timing log file pointer per node
static int base_rank = 0;       // world rank of the base station

void log_time(const char *type, int wrank, int peer, double seconds, int bytes) {
    if (timing_log) fprintf(timing_log, "%s,%d,%d,%.6f,%d,%ld\n", type, wrank, peer, seconds, bytes, (long)time(NULL));
}

// this helper function answers incoming port requests from other nodes
void answer_requests(int free_ports, int *done) {
    int flag, buf[16];
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, STATUS_REQUEST_TAG, MPI_COMM_WORLD, &flag, &status);
    while (flag) {
        MPI_Recv(buf, 1, MPI_INT, status.MPI_SOURCE, STATUS_REQUEST_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Send(&free_ports, 1, MPI_INT, status.MPI_SOURCE, STATUS_REPLY_TAG, MPI_COMM_WORLD);
        MPI_Iprobe(MPI_ANY_SOURCE, STATUS_REQUEST_TAG, MPI_COMM_WORLD, &flag, &status);
    }
    MPI_Iprobe(base_rank, TERMINATE_TAG, MPI_COMM_WORLD, &flag, &status);
    if (flag) {
        MPI_Recv(buf, sizeof(buf), MPI_BYTE, base_rank, TERMINATE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        *done = 1;
    }
}

// this helper function checks for a response from the base station
void check_base_response(int wrank, double *sent_at) {
    int flag, bytes;
    MPI_Status status;
    BaseResponse response;
    MPI_Iprobe(base_rank, RESPONSE_TAG, MPI_COMM_WORLD, &flag, &status);
    if (!flag) return;
    MPI_Get_count(&status, MPI_BYTE, &bytes);
    MPI_Recv(&response, sizeof(BaseResponse), MPI_BYTE, base_rank, RESPONSE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    double delay = MPI_Wtime() - *sent_at;
    log_time("report_rtt", wrank, base_rank, delay, (int)sizeof(NodeReport) + bytes);
    if (response.available_count == 0) {
        printf("Rank %d: base station (rank %d) reports no available capacity, delay=%.2f seconds\n", wrank, base_rank, delay);
    } else {
        printf("Rank %d: forwarding EVs to nodes:", wrank);
        for (int i = 0; i < response.available_count; i++) printf(" %d", response.available_nodes[i]);
        printf("  (reply took %.4f s) \n", delay);
    }
    *sent_at = -1; // not waiting anymore
}

// helper function to wait until a specific time while answering incoming port responses
void wait_until(double end, int free_ports, int wrank, double *sent_at, int *done) {
    while (MPI_Wtime() < end && !*done) {
        answer_requests(free_ports, done);
        if (*sent_at >= 0) check_base_response(wrank, sent_at);
        usleep(100); // this reduces CPU usage by sleeping for a short while
    }
}

// helper function to ask every neighbour for free port count, and store answers
void request_neighbours(NodeReport *r, int free_ports, double end, int *done) {
    MPI_Request request[MAX_NEIGHBOURS];
    MPI_Status status;
    double sent[MAX_NEIGHBOURS], t0 = MPI_Wtime();
    int ask = 0, got = 0, flag, avail;

    for (int i = 0; i < r-> neighbour_count; i++) {
        r->neighbour_availability[i] = -1;
        sent[i] = MPI_Wtime();
        MPI_Isend(&ask, 1, MPI_INT, r->neighbour_ranks[i], STATUS_REQUEST_TAG, MPI_COMM_WORLD, &request[i]);
    }
    while (got < r->neighbour_count && MPI_Wtime() < end && !*done) {
        answer_requests(free_ports, done);
        MPI_Iprobe(MPI_ANY_SOURCE, STATUS_REPLY_TAG, MPI_COMM_WORLD, &flag, &status);
        if (!flag) { usleep(100); continue; }
        MPI_Recv(&avail, 1, MPI_INT, status.MPI_SOURCE, STATUS_REPLY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for (int i = 0; i < r-> neighbour_count; i++) {
            if (r->neighbour_ranks[i] == status.MPI_SOURCE) {
                r->neighbour_availability[i] = avail;
                log_time("status_rtt", r->node_rank, status.MPI_SOURCE, MPI_Wtime() - sent[i], 2 * (int)sizeof(int));
                got++;
            }
        }
    }
    if (got == r->neighbour_count) {
        log_time("exchange", r->node_rank, -1, MPI_Wtime() - t0, 2 * got * (int)sizeof(int));
    }
    MPI_Waitall(r->neighbour_count, request, MPI_STATUSES_IGNORE);
    r->message_count = r->neighbour_count + got;
    r->message_size = r->message_count * sizeof(int);
}

// this helper function decides whether to redirect EVs to a neighbour or report congestion to the base station
void decide(NodeReport *r, int wrank, double *sent_at) {
    int best = -1;
    for (int i = 0; i < r->neighbour_count; i++) {
        if (r->neighbour_availability[i] >= DEFAULT_THRESHOLD &&
            (best < 0 || r->neighbour_availability[i] > r->neighbour_availability[best]))
            best = i;
    }
    if (best >= 0) {
        printf("Rank %d: redirecting EVs to neighbour %d (%d free)\n", wrank, r->neighbour_ranks[best], r->neighbour_availability[best]);
        return;
    }
    r->timestamp = (double)time(NULL); // seconds since the epoch
    double t = MPI_Wtime();
    MPI_Send(r, sizeof(NodeReport), MPI_BYTE, base_rank, REPORT_TAG, MPI_COMM_WORLD);
    *sent_at = MPI_Wtime();
    log_time("report_send", wrank, base_rank, *sent_at - t, (int)sizeof(NodeReport)); // how long the send call blocked
    printf("Rank %d: neighbours full, reported congestion to base station\n", wrank);
}

// helper method to discard any pending MPI messages
void discard_messages(void) {
    int flag, len;
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
    while (flag) {
        MPI_Get_count(&status, MPI_BYTE, &len);
        char *buf = malloc(len > 0 ? len : 1);
        MPI_Recv(buf, len, MPI_BYTE, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        free(buf);
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
    }
}

// main function
int main(int argc, char *argv[]){
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    int wrank, wsize, m = argc > 2 ? atoi(argv[1]) : 3, n = argc > 2 ? atoi(argv[2]) : 3;
    double sim_seconds = argc > 3 ? atof(argv[3]) : DEFAULT_SIMULATION_TIME;
    double window_seconds = argc > 4 ? atof(argv[4]) : DEFAULT_TIME_WINDOW;
    int busy_pct = argc > 5 ? atoi(argv[5]) : 80;      // % chance a port is busy
    unsigned rep = argc > 6 ? atoi(argv[6]) : 0;       // run number, changes the seed
    double cycle_seconds = window_seconds;
    MPI_Comm_rank(MPI_COMM_WORLD, &wrank);
    MPI_Comm_size(MPI_COMM_WORLD, &wsize);
    base_rank = wsize - 1;

    if (wsize != m * n + 1) {
        if (wrank == 0) fprintf(stderr, "Need %d processes\n", m * n + 1);
        MPI_Finalize();
        return 1;
    }

    // rank 0...m*n-1 are charging nodes, last rank is base station
    MPI_Comm nodes, grid;
    MPI_Comm_split(MPI_COMM_WORLD, wrank == base_rank ? MPI_UNDEFINED : 0, wrank, &nodes);
    if (wrank == base_rank) {
        char path[64];
        snprintf(path, sizeof(path), "base_station_%dx%d.log", m, n);
        if (run_base_station(m, n, sim_seconds, window_seconds, path) != 0) MPI_Abort(MPI_COMM_WORLD, 1);
    } else {
        int dimensions[2] = {m, n}, wraparound[2] = {0, 0}, id, xy[2], shift_res[MAX_NEIGHBOURS];
        NodeReport report = {0};

        MPI_Cart_create(nodes, 2, dimensions, wraparound, 0, &grid);
        MPI_Comm_rank(grid, &id);
        MPI_Cart_coords(grid, id, 2, xy);
        MPI_Cart_shift(grid, 0 , 1, &shift_res[0], &shift_res[1]); // move up and down
        MPI_Cart_shift(grid, 1 , 1, &shift_res[2], &shift_res[3]); // move left and right

        report.node_rank = wrank; report.x = xy[0]; report.y = xy[1]; 

        for (int i = 0; i < MAX_NEIGHBOURS; i++) 
            if (shift_res[i] != MPI_PROC_NULL) report.neighbour_ranks[report.neighbour_count++] = shift_res[i];
        printf("Rank %d (%d,%d) neighbours=%d:", wrank, xy[0], xy[1], report.neighbour_count);
        for (int i = 0; i < report.neighbour_count; i++) printf(" %d", report.neighbour_ranks[i]);
        printf("\n");

        char logname[64];
        snprintf(logname, sizeof(logname), "timing_%dx%d_rank%d.csv", m, n, wrank);
        timing_log = fopen(logname, "w");
        if (timing_log) fprintf(timing_log, "type,rank,peer,seconds,bytes,epoch\n");
        else fprintf(stderr, "Rank %d: could not open %s, continuing without timing log\n", wrank, logname);

        int ports[K] = {0}; // 0 is free, 1 if in use
        int done = 0;
        double sent_at = -1;
        #pragma omp parallel num_threads(K) // pragma is used because each port can be handled by a separate thread {}
        {
            int p = omp_get_thread_num();
            unsigned seed = wrank * 100 + p + rep * 10000; // this generates a unique seed for each thread
            for (int c = 0; !done; c++) {
                ports[p] = rand_r(&seed) % 100 < busy_pct; // this simulates the probability that the port is busy
                #pragma omp barrier 
                #pragma omp master
                {
                    double end = MPI_Wtime() + cycle_seconds;
                    int free_ports = 0;
                    for (int i = 0; i < K; i++) free_ports += !ports[i];
                    report.available_ports = free_ports;

                    time_t now = time(NULL); // same layout as Table 1 in the brief
                    struct tm t;
                    localtime_r(&now, &t);
                    printf("Rank %d | %04d %02d %02d %02d %02d %02d | %d free\n", wrank,
                           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, free_ports);

                    if (free_ports < DEFAULT_THRESHOLD && sent_at < 0) {
                        request_neighbours(&report, free_ports, end, &done);
                        if (!done) decide(&report, wrank, &sent_at);
                    }
                    wait_until(end, free_ports, wrank, &sent_at, &done);
                }
                #pragma omp barrier
            }
            
        }
        printf("Rank %d: termination received, shutting down\n", wrank);
        MPI_Barrier(nodes);  // every node has stopped sending
        discard_messages();  // discard anything still queued for us
        if (timing_log) fclose(timing_log); // flush the timing log
        MPI_Comm_free(&grid);
        MPI_Comm_free(&nodes);
        fflush(stdout);
    }
    MPI_Finalize();
    return 0;
}


