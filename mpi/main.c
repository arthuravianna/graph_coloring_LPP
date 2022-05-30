#include <time.h>
#include "mpi.h"
#include "util.h"

#define ERROR_READING_GRAPH 1


#define TAG_GET_N_V         0   // get neighbours of vertex v
#define TAG_COLORS_N_V      1   // get colors of neighbours of vertex v
#define TAG_COLOR_SET_I     2   // color set I with color c
#define TAG_FINESHED        3   // process finished processing



int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Missing argument <filename>\n");
        return 1;
    }

    // mpi variables
    int rank,  size;
    MPI_Status status;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // graph variables
    int **graph = NULL; // matrix representing the graph
    int n; // number of vertex

    if (rank == 0) {
        char *filename = argv[1]; // file containing the graph

        //////////////////////
        // initializing graph
        //////////////////////
        graph = read_file(filename, &n);
        if (!graph) {
            printf("Error reading graph\n");
            MPI_Abort(MPI_COMM_WORLD, ERROR_READING_GRAPH);
            return 0;
        }

        ////////////////////////////////
        // broadcast n to other threads
        ////////////////////////////////
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

        ////////////////////////////////////
        // broadcast graph to other threads
        ////////////////////////////////////
        for (int i = 0; i < n; i++){
            MPI_Bcast(graph[i], n, MPI_INT, 0, MPI_COMM_WORLD);
        }

        MPI_Barrier(MPI_COMM_WORLD); // 1 sync

        //////////////////////////////////////////////
        // Assigning Random Numbers to all vertex.
        // It is done in parallel using MPI_Allgatherv
        //////////////////////////////////////////////
        srand(time(NULL)); // seed of rand

        int rank0_part_n = n / size + n % size;
        int part_n = n / size;
        int w_send[part_n]; // Starting address of send buffer
        int w_recv[n]; // Starting address of recv buffer
        int recvcounts[size]; // number of elements that are received from each process
        int displs[size]; // displacement (relative to recvbuf) at which to place the incoming data from process i

        recvcounts[0] = rank0_part_n;
        for (int i = 1; i < size; i++) recvcounts[i] = part_n;

        for (int i = 0; i < rank0_part_n; i++) w_send[i] = rand(); //w_send[i] = 100 + (rand() % 1000);

        displs[0] = 0;
        for (int i = 1; i < size; i++) displs[i] = ((i-1) * part_n) + rank0_part_n;

        MPI_Allgatherv(w_send, rank0_part_n, MPI_INT, w_recv, recvcounts, displs, MPI_INT, MPI_COMM_WORLD);



        ////////////////////////////////////////////////////////////////
        // Process 0 manages the variables used in the coloring process
        ////////////////////////////////////////////////////////////////
        color[n] = {-1};
        int finished_counter = 0;
        int **resp = malloc(2,sizeof(int*));
        int header[2];
        while (finished != size-1) { // coloring loop
            MPI_Recv(header, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == TAG_GET_N_V) {
                resp = get_neighbours(int **graph, int n, int v);
                
                // Send number of neighbours
                MPI_Send(resp[0], 1, MPI_INT, status.MPI_SOURCE, TAG_GET_N_V, MPI_COMM_WORLD);

                // Send neighbours
                MPI_Send(resp[1], resp[0], MPI_INT, status.MPI_SOURCE, TAG_GET_N_V, MPI_COMM_WORLD);

                free(resp[0]);
                free(resp[1]);
            }
            else if (status.MPI_TAG == TAG_COLORS_N_V) {
                // header = [vertex, NULL]
                resp = get_neighbours_colors(graph, n, header[0], color);
                
                // Send number of colors
                MPI_Send(resp[0], 1, MPI_INT, status.MPI_SOURCE, TAG_COLORS_N_V, MPI_COMM_WORLD);

                // Send neighbours colors
                MPI_Send(resp[1], resp[0], MPI_INT, status.MPI_SOURCE, TAG_COLORS_N_V, MPI_COMM_WORLD);

                free(resp[0]);
                free(resp[1]);
            }
            else if (status.MPI_TAG == TAG_COLOR_SET_I) {
                // header = [color, size of set I]
                int set_I[header[1]];
                MPI_Recv(set_I, header[1], MPI_INT, status.MPI_SOURCE, TAG_COLOR_SET_I, MPI_COMM_WORLD, &status);

                for (int i = 0; i < set_I; i++) {
                    color[set_I[i]] = header[0];
                }
            }
            else if (status.MPI_TAG == TAG_FINESHED) {
                finished_counter++;
            }
        }
    }
    else {
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
        printf("Rank = %d, n = %d\n", rank, n);
        graph = (int **)calloc(n, sizeof(int *));
        for (int i = 0; i < n; i++) {
            graph[i] = (int *)calloc(n, sizeof(int));
            MPI_Bcast(graph[i], n, MPI_INT, 0, MPI_COMM_WORLD);
        }

        MPI_Barrier(MPI_COMM_WORLD); // 1 sync

        // for (int i = 0; i < n; i++) {
        //     for (int j = 0; j < n; j++) printf("graph[%d][%d] = %d\n", i, j, graph[i][j]);
        // }

        //////////////////////////////////////////////
        // Assigning Random Numbers to all vertex.
        // It is done in parallel using MPI_Allgatherv
        //////////////////////////////////////////////
        srand(time(NULL) + rank); // seed of rand

        int rank0_part_n = n / size + n % size;
        int part_n = n / size;
        int w_send[part_n];
        int w_recv[n];
        int recvcounts[size];
        int displs[size];

        recvcounts[0] = rank0_part_n;
        for (int i = 1; i < size; i++) recvcounts[i] = part_n;

        for (int i = 0; i < part_n; i++) w_send[i] = rand(); //w_send[i] = rand() % 99;

        displs[0] = 0;
        for (int i = 1; i < size; i++) displs[i] = ((i-1) * part_n) + rank0_part_n;

        MPI_Allgatherv(w_send, part_n, MPI_INT, w_recv, recvcounts, displs, MPI_INT, MPI_COMM_WORLD);

    }

    for (int i = 0; i < n; i++) {
        free(graph[i]);
    }
    free(graph);
    
    MPI_Finalize();
    return 0;
}