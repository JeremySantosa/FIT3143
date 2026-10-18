
#ifndef COMMON_H
#define COMMON_H

#include <mpi.h>

// Maximum number of neighbours for each charging node
#define MAX_NEIGHBOURS 4

// Maximum number of charging ports per node
#define MAX_PORTS 10

// MPI message tags
#define REPORT_TAG 100
#define RESPONSE_TAG 101
#define TERMINATE_TAG 102
#define STATUS_REQUEST_TAG 103
#define STATUS_REPLY_TAG 104

// Simulation settings (can be changed)
#define DEFAULT_THRESHOLD 1
#define DEFAULT_TIME_WINDOW 10
#define DEFAULT_SIMULATION_TIME 60

// Congestion report sent from a charging node to base station
typedef struct {
    int node_rank;
    int x;
    int y;

    int available_ports;

    // Neighbour information
    int neighbour_ranks[MAX_NEIGHBOURS];
    int neighbour_availability[MAX_NEIGHBOURS];
    int neighbour_count;

    // Communication statistics
    int message_count;
    int message_size;

    // Time congestion was detected
    double timestamp;

} NodeReport;

// Response sent from base station to charging node
typedef struct {
    int reporting_node;

    // Number of nearest available charging nodes
    int available_count;

    // Ranks of available charging nodes
    // Sized for the largest supported grid
    int available_nodes[256];

} BaseResponse;

#endif
