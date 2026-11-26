#!/bin/bash

log_dir="benchmark_trim_logs"
log_runtime="$log_dir/results.csv"
proof_dir_trim="trim_proof"
mkdir -p "$log_dir"
echo -n "formula,runtime_precheck,runtime_trim,runtime_postcheck" > "$log_runtime"

for formula in formulas/debug/*; do
    formula_name="${formula#formulas/debug/}"
    proof_dir="benchmark_proofs/$formula_name"
    echo "run $formula_name"
    echo >> "$log_runtime"
    echo -n "$formula_name," >> "$log_runtime"
    
    ## run check on untrimmed proof
    echo -n "    * precheck.. "
    start=$(date +%s.%N)
    scripts/run/simple_palrup_check.sh "$formula" "$proof_dir" 0 > "$log_dir/precheck_$formula_name"
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo -n "$elapsed," >> "$log_runtime"
    if [[ ! $(grep "s VALIDATED" "$log_dir/precheck_$formula_name") ]]; then
        echo "ERROR"
        echo "    * Skip instance."
        echo -n "," >> "$log_runtime"
        continue
    fi
    echo "DONE"

    ## run trim
    echo -n "    * trim.. "
    start=$(date +%s.%N)
    scripts/run/simple_palrup_trim.sh $proof_dir > "$log_dir/trim_$formula_name"
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo -n "$elapsed," >> "$log_runtime"
    echo "DONE"
    
    ## copy trimmed proof
    echo -n "    * copy .. "
    rm -r "$proof_dir_trim" 2> /dev/null
    for f in $proof_dir/*/*/out.palrup.trim; do
        name=${f#"$proof_dir"}; name=${name%.trim}; mkdir -p $proof_dir_trim/${name%out.palrup}; cp $f $proof_dir_trim/$name
    done
    echo "DONE"

    echo -n "    * postcheck.. "
    start=$(date +%s.%N)
    scripts/run/simple_palrup_check.sh "$formula" "$proof_dir_trim" 0 > "$log_dir/postcheck_$formula_name"
    end=$(date +%s.%N)
    elapsed=$( echo "$end - $start" | bc )
    echo -n "$elapsed" >> "$log_runtime"
    if [[ ! $(grep "s VALIDATED" "$log_dir/postcheck_$formula_name") ]]; then
        echo "ERROR"
        echo "    * ABORT"
        exit 1
    fi
    echo "DONE"
done


