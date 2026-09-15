#!/bin/bash

###################
##
## Launches Pals in a Process on a local machine.
## Designed to be executed independantly on distributed Systems.
##
##################

start=$(date +%s.%N)

child_error() {
    ## TODO: handle cleanup
    echo "Caught abort signal" &>> "$log"
    pkill -P $$     # kill children
    exit 1          # end process
}
trap 'child_error' SIGABRT

# degree of cleanup after checking
#  0: no cleanup
#  1: remove working dir
#  2: remove proof & working dir
cleanup=0
use_drup=0

## TODO: default values
for arg in "$@"; do
    case $arg in
        -num-solvers=*)
            num_solvers="${arg#*=}" ;;
        -num-nodes=*)
            num_nodes="${arg#*=}" ;;
        -num-procs-per-node=*)
            num_proc_per_node="${arg#*=}" ;;
        -proof-palrup=*)
            proof_palrup="${arg#*=}" ;;
        -proof-working=*)
            proof_working="${arg#*=}" ;;
        -log-dir=*)
            log_dir="${arg#*=}" ;;
        -timeout=*)
            timeout="${arg#*=}" ;;
        -use-local-discs=*)
            # set to true if proof is written on distributed local disks
            use_local_discs="${arg#*=}" ;;
        -execution-dir=*)
            working_dir="${arg#*=}" ;;

        #pal
        -formula-path=*)
            formula_path="${arg#*=}" ;;
        -redist-strat=*)
            redist_strat="${arg#*=}" ;;
        -read-buffer-size=*)
            read_buffer_size="${arg#*=}" ;;
        -write-buffer-size=*)
            write_buffer_size="${arg#*=}" ;;
        -merge-buffer-size=*)
            merge_buffer_size="${arg#*=}" ;;
        -q-size=*)
            q_size="${arg#*=}" ;;
        -q-alpha=*)
            q_alpha="${arg#*=}" ;;
        -palrup-binary=*)
            palrup_binary="${arg#*=}" ;;
        -use-drup=*)
            use_drup="${arg#*=}" ;;
        -convert=*)
            convert="${arg#*=}" ;;
        -full-check=*)
            full_check="${arg#*=}" ;;
        -cleanup=*)
            cleanup="${arg#*=}" ;;
        -best-effort=*)
            best_effort="${arg#*=}" ;;
        *)
            echo "Unknown arg $arg"
            exit 1
            ;;
    esac
done
## TODO: check options?

env="NUM_SOLVERS=$num_solvers \
NUM_NODES=$num_nodes \
NUM_PROCS_PER_NODE=$num_proc_per_node \
FORMULA_PATH=$formula_path \
PROOF_PALRUP=$proof_palrup \
PROOF_WORKING=$proof_working \
LOG_DIR=$log_dir \
TIMEOUT=$timeout \
REDIST_STRAT=$redist_strat \
READ_BUFFER_SIZE=$read_buffer_size \
WRITE_BUFFER_SIZE=$write_buffer_size \
MERGE_BUFFER_SIZE=$merge_buffer_size \
Q_SIZE=$q_size \
Q_ALPHA=$q_alpha \
PALRUP_BINARY=$palrup_binary \
USE_LOCAL_DISKS=$use_local_discs \
USE_DRUP=$use_drup \
CONVERT=$convert \
FULL_CHECK=$full_check \
CLEANUP=$cleanup \
BEST_EFFORT=$best_effort"

if [[ $working_dir ]]; then cd $(pwd)/$working_dir; fi

glob_start=$(date +%s.%N)
check_timeout() {
    curr_time=$(date +%s.%N)
    if (( $( echo "($curr_time - $glob_start) > $timeout" | bc ) )); then
        echo "TIMEOUT in process of global_id=$global_id"
        echo "TIMEOUT" &>> "$log"
        exit 1
    fi
    if [[ -d "$proof_working/.error" ]]; then
        echo "ERROR detected" &>> "$log"
        exit 1
    fi
}

num_processes=$(($num_nodes*$num_proc_per_node))

if [[ $use_local_disks -eq 1 ]]; then
    # get local id on node
    for i in $(seq 0 $(($num_proc_per_node-1))); do
        if mkdir /tmp/.pal_launcher.$i.lock 2>/dev/null ; then
            local_id=$i
            break
        fi
    done
else
    for i in $(seq 0 $(($num_processes-1))); do
        if mkdir $proof_working/.pal_launcher.$i.lock 2>/dev/null ; then
            global_id=$i
            local_id=$i
            break
        fi
    done
fi

# fail save
if [[ ! $local_id ]]; then
    >&2 echo "Could not find a local id. Abort."
    exit 1
fi

# calculate comm_size
root=$(echo "sqrt ( $num_solvers )" | bc -l)
root_floor=${root%.*}
comm_size=$(($root_floor**2))
if (( $comm_size < num_solvers )); then
    root_floor=$(($root_floor+1))
    comm_size=$(($root_floor**2))
fi

##########################################
## Calculate list of pals to be spawned ##
##########################################
# get fragments on locally readable disk
# soring is not strictly necessary but helps with debugging
frag_id_set=($(seq 0 $(($num_solvers-1))))
pals_per_proc=$(($num_solvers/$num_processes))

# global_id is still undefined for distributed disks
if [[ $use_local_disks -eq 1 ]]; then
    frag_id_set=($(find $proof_palrup -mindepth 2 -maxdepth 2 -type d | xargs -- basename -a | sort -n))
    global_id=$(((${frag_id_set[0]}/$pals_per_proc)+$local_id))
fi

# generate list of pals corresponding to fragments on local disk
frag_pals_start_idx=$(($local_id*$pals_per_proc))
frag_pals=${frag_id_set[@]:frag_pals_start_idx:pals_per_proc}

# generate list of additional pals needed in reroute step
num_comm_pals=$(((($comm_size-$num_solvers)/$num_processes)+1))
comm_pal_start_idx=$((num_solvers+(global_id*num_comm_pals)))
comm_pal_end_idx=$(($comm_pal_start_idx+$num_comm_pals-1))
comm_pals=($(for i in $(seq $comm_pal_start_idx $comm_pal_end_idx); do echo $i; done))

# concatenated list of all pals to be spawned
pal_id_set=(${frag_pals[@]} ${comm_pals[@]})

# create log
mkdir -p "$log_dir/$global_id"
log="$log_dir/$global_id/palrup.out"
if [[ $use_drup -eq 1 ]]; then log="$log_dir/$global_id/palrup.out"; fi

# Make mapping between mpi-rank and global_id possible
echo "Created pal_launcher with global_id:$global_id, local_id:$local_id"

echo "Initiated Pal launcher with global_id: $global_id and local_id: $local_id" &>> "$log"
echo "num_comm_pals: $num_comm_pals" &>> "$log"
echo "frag_pals: ${frag_pals[@]}" &>> "$log"
echo "comm_pals: ${comm_pals[@]}" &>> "$log"
echo "pal_id_set: ${pal_id_set[@]}" &>> "$log"
echo "read env variables:" &>> "$log"
echo "num_solvers: $num_solvers" &>> "$log"
echo "num_nodes: $num_nodes" &>> "$log"
echo "num_proc_per_node: $num_proc_per_node" &>> "$log"
echo "proof_palrup: $proof_palrup" &>> "$log"
echo "proof_working: $proof_working" &>> "$log"
echo "log_dir: $log_dir" &>> "$log"
echo "timeout: $timeout" &>> "$log"
echo "use_local_disks: $use_local_disks" &>> "$log"
echo "cleanup: $cleanup" &>> "$log"
echo "use_drup: $use_drup" &>> "$log"

echo "prepare working and log directories" &>> "$log"
for pal_id in ${pal_id_set[@]}; do
    if [[ $pal_id -ge $comm_size ]]; then continue; fi
    dir_hierarchy=$(($pal_id/$root_floor))
    dir_hierarchy=${dir_hierarchy%.*}
    mkdir -p "$proof_working/$dir_hierarchy/$pal_id"
    mkdir -p "$log_dir/pals/$dir_hierarchy/"
done

################
## start pals ##
################
launch_pal() {
    cmd="$env bash build/pal.sh $1"
    echo "Launch Pal $1: $cmd" &>> "$log"
    eval $cmd &>> "$log"
    local res=$?
    
    if [[ $res -ne 0 ]]; then
        echo "ERROR in pal $1" &>> "$log"
        mkdir -p "$proof_working/.error/$1" 2>/dev/null
        kill -s SIGABRT $$    # kill parent
    fi
}

echo "Launch Pals.." &>> "$log"
for pal in ${pal_id_set[@]}; do
    launch_pal $pal &
done
echo "Wait for Pals.." &>> "$log"
wait
echo "All Pals returned." &>> "$log"


# clean up after everything finished
if [[ $global_id == 0 ]]; then
    echo "wait for all global Pals to be finished" &>> "$log"
    # pals finish in binary tree order, i.e. pal 0 always finishes last
    until [[ -d "$proof_working/0/0/.done" ]]; do
        check_timeout
        sleep 0.2
    done

    # check for global validity
    echo "check for validity" &>> "$log"
    if [[ -d "$proof_working/.unsat_found" && -d "$proof_working/0/0/.valid" ]]; then
        success_file_name="success.palrup"
        if [[ $use_drup -eq 1 ]]; then success_file_name="success.palrup"; fi
        echo "PROOF VALIDATED" > "$log_dir/$success_file_name"
        echo "PROOF_VALIDATED" &>> "$log"
    fi

    mkdir -p $proof_working/.cleanup
    mkdir -p $proof_working/.DONE
fi

glob_end=$(date +%s.%N)
elapsed=$(echo "$glob_end - $glob_start" | bc -l)
echo "GLOB_WC_TIME=$elapsed" &>> "$log"

echo "Release lock" &>> "$log"
if [[ $use_local_disks -eq 1 ]]; then
    rmdir /tmp/.pal_launcher.$local_id.lock 2>/dev/null
else
    rmdir $proof_working/.pal_launcher.$local_id.lock 2>/dev/null
fi

echo "FINISHED" &>> "$log"
if [[ $cleanup -eq 0 ]]; then exit 0; fi

# Wait for cleanup
echo "wait for cleanup" &>> "$log"
start=$(date +%s.%N)
until [[ -d "$proof_working/.cleanup" ]]; do
    check_timeout
    sleep 0.1
done
end=$(date +%s.%N)
elapsed=$( echo "$end - $start" | bc )
echo "CLEANUP_WAIT_WC_TIME=$elapsed" &>> "$log"

# cleanup dirs of all pals
echo "cleanup Pals' directories.." &>> "$log"
start=$(date +%s.%N)
for pal in ${pal_id_set[@]}; do
    dir_hierarchy=$(($pal/$root_floor))
    if [[ $cleanup -gt 0 ]]; then
        rm -r "$proof_working/$dir_hierarchy/$pal" 2>/dev/null &
    fi
    if [[ $cleanup -gt 1 ]]; then
        rm -r "$proof_palrup/$dir_hierarchy/$pal" 2>/dev/null &
    fi
done
wait
echo "all Pals' directories cleaned up" &>> "$log"

if [[ $global_id == 0 ]]; then
    echo "clean up hierarchies"
    # wait for all pal dirs to be cleaned up
    empty="true"
    for i in $(seq 0 $(($root_floor-1))); do
        if [[ $cleanup -gt 0 && $(ls $proof_working/$i) ]]; then empty=""; fi
    done

    if [[ $empty || $cleanup -gt 1 ]]; then
        # clean up dir hierarchy and .unsat_found
        if [[ $cleanup -gt 0 ]]; then rm -r $proof_working; fi

        # clean up proof hierarchy
        if [[ $cleanup -gt 1 ]]; then rm -r $proof_palrup; fi
    else
        echo "Working dir not empty. Abandon cleanup." &>> "$log"
    fi
fi

end=$(date +%s.%N)
elapsed=$( echo "$end - $start" | bc )
echo "CLEANUP_WC_TIME=$elapsed" &>> "$log"
