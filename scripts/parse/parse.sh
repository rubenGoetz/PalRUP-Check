#!/bin/bash

#################################################
## Bash wrapper for parse_logs.py              ##
## Expects path to a log directory as argument ##
#################################################

# TODO: give path to Python venv directory
VENV=".venv"

if [[ ! $1 ]]; then
    echo "No path was given. ABORT."
    exit 1
fi

source "$VENV/bin/activate"
python3 $(dirname "$0")/parse_logs.py $1
deactivate
