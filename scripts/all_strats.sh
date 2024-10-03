#!/bin/bash

# Define the values for gshr-strat and shr-strat
gshr_values=(1 2 3)
shr_values=(1 2 3 4 5)

# Define other parameters
c_val=10
t_val=50
v_val=2
solver_val="k"

# Loop through each combination
# for gshr in "${gshr_values[@]}"; do
for shr in "${shr_values[@]}"; do
    echo "Running ./painless with gshr-strat=$gshr and shr-strat=$shr"
    ./painless -c=$c_val -t=$t_val -v=$v_val -solver=$solver_val -shr-strat=$shr -gshr-strat=$gshr $1
    wait $!
    echo "------------------------------------------------------------"
done
# done
