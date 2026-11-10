#!/bin/bash

#PROOF_DIR="proofs/r3unsat_300"        # Directory containing proof
PROOF_DIR="proofs/proof#1"
#WORKING_DIR="../mallob/proof_working"                 # Directory for PalRUP communication
WORKING_DIR="working"
NUM_SOLVERS=12                      # Number of proof fragments

# Calculate comm_size = ceil(sqrt(NUM_SOLVERS))^2
# Equivalent C++ code:  std::pow(std::ceil(std::sqrt((double) _setup.maxNumSolvers)), 2);
root=$(echo "sqrt ( $NUM_SOLVERS )" | bc -l)
root_floor=${root%.*}
comm_size=$(($root_floor**2))
root_ceil=$root_floor
if (( $comm_size < $NUM_SOLVERS )); then
    root_ceil=$(($root_floor+1))
    comm_size=$(($root_ceil**2))
fi

# Ceate hierarchy for working directory
rm -r "$WORKING_DIR"
for id in $(seq 0 $((comm_size-1))); do
    dir="$WORKING_DIR/$(($id/$root_ceil))/$id"
    mkdir -p $dir
done

# Run local imported clause extraction step for each solver [0, NUM_SOLVERS)
for id in $(seq 0 $((NUM_SOLVERS-1))); do
    # Run each local extraction sequencially, uncomment '&' and 'wait' to run them in parallel
    build/palrup_extract_imports \
        -palrup-path=$PROOF_DIR -working-path=$WORKING_DIR \
        -num-solvers=$NUM_SOLVERS -pal-id=$id &
    if (( $? != 0 )); then exit 1; fi
done
wait

# Run redistribution of imported clauses for each process [0, ceil(sqrt(NUM_SOLVERS))^2)
# Note comm_size instead of NUM_SOLVERS
for id in $(seq 0 $((comm_size-1))); do
    # Run each redistribution sequencially, uncomment '&' and 'wait' to run them in parallel
    build/palrup_redistribute_trim \
        -working-path=$WORKING_DIR -num-solvers=$NUM_SOLVERS \
        -pal-id=$id &
    if (( $? != 0 )); then exit 1; fi
done
wait

#exit

# Run local trimming step for each solver [0, NUM_SOLVERS)
mpiexec -n $NUM_SOLVERS --oversubscribe build/palrup_local_trim $PROOF_DIR $WORKING_DIR
