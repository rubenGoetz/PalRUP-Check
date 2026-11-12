#!/bin/bash

CNF_PATH=formulas/r3unsat_300.cnf  # Original Formula
#CNF_PATH="formulas/debug/944b4eaecefa1d15c2d2a8ac0c2d1c31-b14.cnf"
#CNF_PATH="formulas/debug/a70883771fd1c210d94a916d52510a3a-gm28sparrc.cnf"
#PROOF_DIR="buggy_proof"        # Directory containing proof
#PROOF_DIR="error"
#PROOF_DIR="benchmark_proofs/a70883771fd1c210d94a916d52510a3a-gm28sparrc.cnf"
#PROOF_DIR="proofs/palrup/r3unsat_300"
#PROOF_DIR="../mallob/proofs/proof#1"
PROOF_DIR="trim"
#PROOF_DIR="proofs/converted"
#WORKING_DIR="../mallob/proof_working"                 # Directory for PalRUP communication
WORKING_DIR="working"
NUM_SOLVERS=12                      # Number of proof fragments
DRUP=0
CONVERT=1

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

# Create hierarchy for working directory
rm -r "$WORKING_DIR"
for id in $(seq 0 $((comm_size-1))); do
    dir="$WORKING_DIR/$(($id/$root_ceil))/$id"
    mkdir -p $dir
done

# Run local checking step for each solver [0, NUM_SOLVERS)
for id in $(seq 0 $((NUM_SOLVERS-1))); do
    #if [[ $id -eq 0 ]]; then id=37; fi
    #if [[ $id -eq 37 ]]; then id=0; fi
    # Run each local solver sequencially, uncomment '&' and 'wait' to run them in parallel
    if [[ $CONVERT -eq 1 ]]; then
        build/palrup_local_check -formula-path=$CNF_PATH -palrup-path=$PROOF_DIR -working-path=$WORKING_DIR -num-solvers=$NUM_SOLVERS -pal-id=$id -drup=$DRUP -convert-to-lrup=$CONVERT &
    else
        build/palrup_local_check_fast_rup -formula-path=$CNF_PATH -palrup-path=$PROOF_DIR -working-path=$WORKING_DIR -num-solvers=$NUM_SOLVERS -pal-id=$id -drup=$DRUP -convert-to-lrup=$CONVERT &
    fi
    if (( $? != 0 )); then break; fi
done
wait
#exit

# Run redistribution of imported clauses for each process [0, ceil(sqrt(NUM_SOLVERS))^2)
# Note comm_size instead of NUM_SOLVERS
for id in $(seq 0 $((comm_size-1))); do
    # Run each redistribution sequencially, uncomment '&' and 'wait' to run them in parallel
    build/palrup_redistribute -working-path=$WORKING_DIR -num-solvers=$NUM_SOLVERS -pal-id=$id #&
done
#wait

# Run local checking step for each solver [0, NUM_SOLVERS)
for id in $(seq 0 $((NUM_SOLVERS-1))); do
    # Run each local confirmation sequencially, uncomment '&' and 'wait' to run them in parallel
    build/palrup_confirm -palrup-path=$PROOF_DIR -working-path=$WORKING_DIR -num-solvers=$NUM_SOLVERS -pal-id=$id -drup=$DRUP #&
done
#wait

# Validate
# Count ./check_ok directories and check existence of .unsat_found directory
# ls is not the most stable of commands though
if [[ $(ls $WORKING_DIR/*/*/.check_ok | wc -w) -eq $NUM_SOLVERS && -d $WORKING_DIR/.unsat_found ]]; then
    echo "s VALIDATED"
else
    echo "s NOT VALIDATED"
fi
