
#include <mpi.h>

#include "options.h"
#include "local_trim.hpp"

void run_trim(std::string palrup_path, std::string working_path) {
    ProofTrimmer(palrup_path, working_path, 1024, 1024).trim();
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    // DEBUG Barrier
    
    // int rank;
    // MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    // std::cout << "[" << rank << "] PID: " << getpid() << std::endl;
    // MPI_Barrier(MPI_COMM_WORLD);
    // if (rank == 0) {
    //     std::cout << "Press Enter to continue.\n";
    //     std::ignore = std::getchar();
    // }
    // MPI_Barrier(MPI_COMM_WORLD);

    ProofTrimmer(argv[1], argv[2], 1024 * 1024, 1024 * 1024).trim();

    MPI_Finalize();
    return 0;
}

