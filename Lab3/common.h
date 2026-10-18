
#ifndef COMMON_H
#define COMMON_H

#include <mpi.h>

// Maximum number of neighbours and charging ports
#define MAX_NEIGHBOURS 4
#define MAX_PORTS 10

// MPI message tags
#define REPORT_TAG 100
#define RESPONSE_TAG 101
#define TERMINATE_TAG 102
#define STATUS_REQUEST_TAG 103
#define STATUS_REPLY_TAG 104

// Default simulation settings
#define DEFAULT_THRESHOLD 1
#define DEFAULT_TIME_WINDOW 10
#define DEFAULT_SIMULATION_TIME 60

// Maximum number of nodes in a base station response
#define MAX_RESPONSE_NODES 256

// Congestion report sent from an EV charging node
// to the base station
typedef struct {
    int node_rank;
    int x;
    int y;
    int available_ports;

    // Neighbouring node information
    int neighbour_ranks[MAX_NEIGHBOURS];
    int neighbour_availability[MAX_NEIGHBOURS];
    int neighbour_count;

    // Communication statistics
    int message_count;
    int message_size;

    // Time when congestion was detected
    double timestamp;

} NodeReport;

// Response sent from the base station
// to an EV charging node
typedef struct {
    int reporting_node;
    int available_count;
    int available_nodes[MAX_RESPONSE_NODES];

} BaseResponse;

#endif // COMMON_H
