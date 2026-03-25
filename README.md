
# PalRUP-Check

PalRUP-Check is a standalone proof checker for the **PalRUP** (Parallel LRUP) framework and thus enables scalable distributed proof checking.

A PalRUP proof consists of $n$ *proof fragments* that are usually created by $n$ solver threads of a clause sharing solver.

<!-- TODO: link paper when published -->

# Setup

To build PalRUP-Check, run `bash scripts/setup/build` or execute the following code manually:
```
# create build directory
mkdir -p build
cd build

# build all targets
cmake ..
make -j

# return to base directory
cd ..
```

To test if PalRUP-Check was properly built, execute `./test_full_run` in the `build/` directory. This will execute a full PalRUP check on one of the provided example proofs.

To execute all unit tests execute `make test` in the `build/` directory.

# Execute

PalRUP-Check consists of three main executables: **palrup_local_check**, **palrup_redistribute** and **palrup_confirm** that can be run in parallel and on distributed systems. 

To successfully check a PalRUP proof of $n$ fragments, $n$ *palrup_local_check* processes have to be executed, followed by $\lceil \sqrt{n} \rceil$ *palrup_redistribute* processes, followed by $n$ *palrup_confirm* processes. If a PalRUP proof was checked successfully, the working directory will contain a `.unsat_found` directory in addition to $n$ `.check_ok` directories for each fragment. If any or all of these directories are missing, the proof is not checked properly.

In the following we describe how to successfully run such a pipeline in parallel.

## Via Provided Pals

The recommended way to check a PalRUP proof, is to utilize the same configuration as was used for solving. Say we produced a PalRUP proof by executing a clause sharing solver on $p$ processes containing $n/p$ solver backends each.

We can then execute `bash build/pal_launcher.sh` for each process. If given the appropriate context, the pal_launchers will spawn corresponding *pals* that coordinate and synchronize amongst themselfs. We have to make sure to give the context expected by both the `pal_launcher.sh` and `pal.sh` scripts like so:
```
# navigate to build directory
cd build

# set context
NUM_SOLVERS=n
NUM_NODES=<number of used compute nodes>
NUM_PROCS_PER_NODE=<number of processes per node>
FORMULA_PATH=<path/to/formula>
PROOF_PALRUP=<path/to/proof>
PROOF_WORKING=<path/to/working/dir>
LOG_DIR=<path/to/log/dir>
TIMEOUT=<timeout in s>
USE_LOCAL_DISKS=0

# start process
bash pal_launcher.sh
```
There are optional parameters for a more precise parametrisation. See [pal.sh](./scripts/pal/pal.sh) for details.

In Addition to the actual proof checking, the pals will validate whether the proof was checked successfully. If so, a file called `success.palrup` will be created in the log directory.

**Causion:** In the standard configuration, the pals will also perform an aggressive cleanup procedure, i.e. will delete any created files as well as the actual proof! If this is not desired, the respective code has to be removed from both [`pal.sh`](./scripts/pal/pal.sh) as well as [`pal_launcher.sh`](./scripts/pal/pal_launcher.sh).

## Manually

In Principle, all stages for every fragment can be run independently. However, when running a proof check manually we will have to make sure that all preconditions for *palrup_redistribute* and *palrup_confirm* are met respectively. To assure this conservatively, execute a stage for all proof fragments (in parallel) and wait for all processes to finish before starting the next stage.

# Realated Work

PalRUP-Check is integrated into the massively parallel distributed SAT-solver [MallobSat](https://github.com/domschrei/mallob). For an end-to-end execution of solving, proof logging, proof checking and validation see MallobSats' documantation.
