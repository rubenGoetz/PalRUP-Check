#!/bin/bash
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=1
#SBATCH -t 00:10:00
#SBATCH -p test
#SBATCH --account=  # TODO: set project name
#SBATCH -J parse-logs

#################################################
## Bash wrapper for parse_logs.py              ##
#################################################

# TODO: give path to Python venv directory
VENV=".venv"

# DIR can alternatively be given via command line argument
DIR=""

if [[ ! $DIR ]]; then
    if [[ ! $1 ]]; then
        echo "No path was given. ABORT."
        exit 1
    fi
    DIR=$1
fi

# TODO: replace $(dirname "$0") with absolute path for Slurm job
$VENV/bin/python3 $(dirname "$0")/parse_logs.py $DIR
