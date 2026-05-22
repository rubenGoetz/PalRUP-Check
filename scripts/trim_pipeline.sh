#!/bin/bash

###############################################
## executes complete proof trimming pipeline ##
###############################################

root=$(dirname "$0")/..
solvers=12
sqrt=4

proof="$root/proofs/r3unsat_300/"
if [[ $1 ]]; then proof=$1; fi

check_success() {
    if [[ $1 -ne 0 ]]; then exit 1; fi
}

echo "* clean trim dir"
rm -r "$root/proofs/trim"
for i in $(seq 0 $((solvers-1))); do
    mkdir -p "$root/proofs/trim/$((i/$sqrt))/$i"
    check_success $?
done

echo "* clean working dir"
rm -r "$root/working"
for i in $(seq 0 $(((sqrt*sqrt)-1))); do
    mkdir -p "$root/working/$((i/$sqrt))/$i"
    check_success $?
done

echo "* extract imports"
for i in $(seq 0 $((solvers-1))); do
    $root/build/palrup_extract_imports \
    -palrup-path="$root/proofs/r3unsat_300" -working-path="$root/working" \
    -num-solvers=$solvers -pal-id=$i \
    -q-size-KB=80
    check_success $?
done

echo "* redistribute trim"
for i in $(seq 0 $(((sqrt*sqrt)-1))); do
    $root/build/palrup_redistribute_trim \
    -working-path="$root/working" \
    -num-solvers=$solvers -pal-id=$i
    check_success $?
done

echo "* local trim"
mpiexec -np $solvers --oversubscribe $root/build/palrup_local_trim "$root/proofs/r3unsat_300" "$root/working"
check_success $?

echo "* copy trim"
for i in $(seq 0 $((solvers-1))); do
    cp "$proof/$((i/sqrt))/$i/out.palrup.trim" "$root/proofs/trim/$((i/sqrt))/$i/out.palrup"
    check_success $?
done

echo "* clean .unsat_found flag"
rm -r "$root/working/.unsat_found"
check_success $?

echo "* local check"
for i in $(seq 0 $((solvers-1))); do
    $root/build/palrup_local_check \
    -formula-path="$root/formulas/r3unsat_300.cnf" \
    -palrup-path="$root/proofs/trim" -working-path="$root/working" \
    -num-solvers=$solvers -pal-id=$i
    check_success $?
done

echo "* redistribute"
for i in $(seq 0 $(((sqrt*sqrt)-1))); do
    $root/build/palrup_redistribute \
    -working-path="$root/working" \
    -num-solvers=$solvers -pal-id=$i
    check_success $?
done

echo "* confirm"
for i in $(seq 0 $((solvers-1))); do
    $root/build/palrup_confirm \
    -palrup-path="$root/proofs/trim" -working-path="$root/working" \
    -num-solvers=$solvers -pal-id=$i
    check_success $?
done

echo "* validate"
if [[ -d "$root/working/.unsat_found" && ( $(find $root/working -name .check_ok | wc -l) -eq $solvers ) ]]; then
    echo "* VALIDATED"
else
    echo "* NOT validated"
fi
