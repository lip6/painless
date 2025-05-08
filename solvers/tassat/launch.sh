#!/bin/bash

# Themes for output styling
colored=1
success_theme="\e[1;42;97m"
error_theme="\e[1;101;97m"
info_theme="\e[1;44m"
reset_theme="\e[0m"
log_theme="\033[1;34m"

# Get the directory of the script
script_dir=$(dirname "$(realpath "$0")")

# Function to display an error message and exit
handle_error() {
    echo -e "${error_theme}Error: $1${reset_theme}"
    exit 1
}

# Check if the correct number of arguments is provided
if [ $# -ne 1 ]; then
    handle_error "Usage: $0 <file_with_list_of_cnf_files>"
fi

# Input file containing the list of CNF files
cnf_list_file="$1"

# Check if the CNF list file exists
if [ ! -f "$cnf_list_file" ]; then
    handle_error "The file '$cnf_list_file' does not exist or is not a file."
fi

# Path to the solver executable
solver="$script_dir/tassat"

# Check if the solver is executable
if [ ! -x "$solver" ]; then
    handle_error "Solver executable '$solver' not found or not executable."
fi

# Default parameters
maxTries=1
maxFlips=1000000000
seed=0
verbosity=0

# Timestamped output directory
timestamp=$(date +"%Y%m%d_%H%M%S")
output_dir="$script_dir/../outputs/output_$timestamp"
logs_dir="$output_dir/logs"
errs_dir="$output_dir/errs"
mkdir -p "$logs_dir"
mkdir -p "$errs_dir"

# Create a description file
description_file="$output_dir/description.txt"
{
    echo "Run Timestamp: $timestamp"
    echo "Solver: $solver"
    echo "Parameters:"
    echo "  maxTries: $maxTries"
    echo "  maxFlips: $maxFlips"
    echo "  seed: $seed"
    echo "Input Files:"
    cat "$cnf_list_file"
} > "$description_file"

times_file="$output_dir/times.csv"
echo "instance,par2,answer,flips,flipsPerSecond,r_unsats" > $times_file

# Read the CNF files into an array
mapfile -t cnf_files < "$cnf_list_file"

# Total number of CNF files
total_files=${#cnf_files[@]}

if [ "$total_files" -eq 0 ]; then
    handle_error "No CNF files found in the input file."
fi

# Progress bar function
print_progress() {
    local current=$1
    local total=$2
    local progress=$((current * 100 / total))
    local bar_width=50
    local filled=$((progress * bar_width / 100))
    printf "\r["
    printf "%${filled}s" | tr ' ' '#'
    printf "%$((bar_width - filled))s] %d%% (%d/%d)" "" "$progress" "$current" "$total"
}

# Loop through each CNF file and process it
echo -e "${info_theme}Processing CNF files...${reset_theme}"
current_file=0
for cnf_file in "${cnf_files[@]}"; do
    ((current_file++))
    
    # Print progress
    print_progress "$current_file" "$total_files"
    
    # Check if the file exists
    if [ ! -f "$cnf_file" ]; then
        handle_error "The file '$cnf_file' does not exist or is not a file."
    fi

    # Extract file name and prepare log files
    instance_name=$(basename "$cnf_file" .cnf)
    log_file="$logs_dir/log_$instance_name.txt"
    error_file="$errs_dir/err_$instance_name.txt"
    answer="UNKNOWN"

    # Record the start time
    start_time=$(date +%s.%N)

    # Run the solver
    timeout --preserve-status 5000 "$solver" -v $cnf_file "$seed" "$maxFlips" 1> "$log_file" 2>"$error_file" &
    pid=$!

    wait $pid
    exit_code=$?
    
    # Record the end time
    end_time=$(date +%s.%N)
    time_spent=$(echo "$end_time - $start_time" | bc)

    # Check the exit code of the solver
    #if [[ "$exit_code" -ne 0 && "$exit_code" -ne 10 && "$exit_code" -ne 20 ]]; then
    #    echo -e "\n${error_theme}Solver failed with exit code $exit_code on file: $cnf_file${reset_theme}"
   #     exit 1
   # fi

    # Verify the solution if the solver returns 10
    if [ "$exit_code" -eq 10 ]; then
        output=$("$script_dir/SAT" "$cnf_file" "$log_file" 2>/dev/null)
        last_line=$(echo "$output" | tail -n 1)
        if [ "$last_line" = "11" ]; then
            echo -e "\n${success_theme}SAT solution is correct for file: $cnf_file${reset_theme}"
        elif [ "$last_line" = "-1" ]; then
            echo -e "\n${error_theme}SAT solution is wrong for file: $cnf_file${reset_theme}"
            exit 1
        fi
        answer="SAT"
    else
        time_spent=$(bc <<< "$time_spent * 2")
        echo -e "\n${log_theme}Solver did not return SAT/UNSAT for file: $cnf_file. Skipping verification.${reset_theme}"
    fi

read -r flips flipsPerSecond unsats < <(awk '
    match($0, /c ([0-9]+) flips, ([0-9.]+) thousand flips per second/, a) {
        flips=a[1]
        fps=a[2]
    }
    match($0, /c final minimum of ([0-9]+) unsatisfied clauses/, b) {
        unsats=b[1]
    }
    END {
        print flips, fps, unsats
    }
' $log_file)

    # Save the time spent to the log
    echo "$instance_name,$time_spent,$answer,$flips,$flipsPerSecond,$unsats" >> "$times_file"

    # Display the time spent
    echo -e "${log_theme}Time spent on file $cnf_file: ${time_spent} seconds${reset_theme}"
done

# Print completion message
echo -e "\n${info_theme}All CNF files were processed. Logs saved in $output_dir${reset_theme}"
