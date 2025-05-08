#! /usr/bin/bash

script_dir=$(dirname $0)

source $script_dir/myLibrary.sh

# themes
colored=1
success_theme="\e[1;42;97m"
error_theme="\e[1;101;97m"
info_theme="\e[1;44m"
reset_theme="\e[0m"
log_theme="\033[1;34m"

cleanup() {
    # Check and kill only painless_release or painless_debug processes
    if pgrep -f "painless_(release|debug)" > /dev/null; then
        echo -e "${log_theme}Terminating painless processes...${reset_theme}"
        pkill -9 -f "painless_(release|debug)"
        echo -e "${success_theme}Cleanup completed.${reset_theme}"
    else
        echo -e "${log_theme}No painless processes found. No cleanup needed.${reset_theme}"
    fi
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for required commands
if ! command_exists timeout; then
    echo "Error: 'timeout' command not found. Please install it or use an alternative method."
    exit 1
fi

# Error handling function
handle_error() {
    echo "${error_theme} Error: $1 ${reset_theme}"
    exit 1
}

if [ $# -lt 1 ]; then
    echo "Please enter the file with path to the formulae"
    exit 1
fi

if [ $# -lt 2 ]; then
    echo "Usage: $0 <path_to_parameters_file> <path_to_input_file>"
    exit 1
fi

parameters=$1
input_files=$2

if [ ! -f "$parameters" ]; then
    echo "Parameter file $parameters does not exist or is not a file"
    exit 1
fi

if [ ! -f "$input_files" ]; then
    echo "Input file $input_files does not exist or is not a file"
    exit 1
fi

# info variables
nb_clauses=0

source "$parameters"

if [ ! -v hostfile ]; then
    echo -e "$error_theme hostfile variable is not defined $reset_theme"
    hostfile="$script_dir/hostfile.sh" # Set a default value
fi

if [ $gstrat -ge 0 ] && [ ! -f "$hostfile" ]; then
    default_hostfile="localhost slots=$nb_physical_cores"
    echo -e "$error_theme hostfile variable '$hostfile' doesn't link to a file $reset_theme"
    echo "$default_hostfile" > "$hostfile"
fi

# results
total_time_spent=0
total_mypar2=0
nbSAT=0
nbUNSAT=0
nbTIMEOUT=0
nbUNKNOWN=0
start_time=0
end_time=0
result="UNKNOWN"

#duplicate shell 1 in shell 5 so that even after piping to awk the output is printed via shell 5 (which is now the first)
# exec 5>&1

# Init output files
if [ ! -d "./outputs" ]; then
    echo "Creating outputs directory..."
    mkdir outputs
fi

times_file=./outputs/times.csv

if [ ! -f $times_file ]; then
    echo "bin-param,time,myPAR2,nbSAT,nbUNSAT,nbResolved,nbTIMEOUT,nbUNKNOWN,nbUnresolved" >$times_file
fi

# Per configuration metrics
instance_name="${solver}_L${lstrat}_G${gstrat}${dist}-$(date +"%d_%m_%Y-%H_%M")_$3"
metric_dir="./outputs/metric_$instance_name"
if [ ! -d $metric_dir ]; then mkdir $metric_dir; fi

# Logs dir in metric folder

# Init output files
if [ ! -d "$metric_dir/logs" ]; then
    echo "Creating outputs directory..."
    mkdir $metric_dir/logs
fi

# localSharersInfoFile="$metric_dir/lsharer_L${lstrat}_G${gstrat}_B${nb_bloom_gstrat}${dup}${dist}.csv"
# globalSharersInfoFile="$metric_dir/gsharer_L${lstrat}_G${gstrat}_B${nb_bloom_gstrat}${dup}${dist}.csv"
timesPerIntance="$metric_dir/times_L${lstrat}_G${gstrat}_B${nb_bloom_gstrat}${dup}${dist}.csv"

# create/ files if needed
# if [ ! -f $localSharersInfoFile ]; then
#     echo "nodeID-sharerID,receivedCls,sharedCls,receivedDuplicas" >$localSharersInfoFile
# fi

# if [ ! -f $globalSharersInfoFile ]; then
#     echo "nodeID-sharerID,receivedCls,sharedCls,receivedDuplicas,sharedDuplicasAvoided,messagesSent" >$globalSharersInfoFile
# fi

if [ ! -f $timesPerIntance ]; then
    echo "instance,par2,result,winnerType" >$timesPerIntance
fi

debugmpi=""

if [ $# -gt 3 ] && [ "$4" = "debug" ]; then
    cmd="$script_dir/../build/debug/painless_debug"
    debugmpi="--verbose --debug-daemons"
else
    cmd="$script_dir/../build/release/painless_release"
fi

args="-v=$verbose -c=$nb_solvers -solver=$solver -t=$timeout -shr-strat=$lstrat -shr-sleep=$shr_sleep $flags"

if [ $gstrat -ge 0 ]; then
    grid5000="--mca pml ^ucx"
    # bind-to none to not have all the threads binded to one core as it is when nb process <= 2
    # map $nb_procs processes to each node, and each process is bound to $nb_solvers hardware threads
    # can set timeout with --timeout $timeout
    cmd=$(echo -n "mpirun --hostfile $hostfile \
    $grid5000 ${debugmpi}\
    --mca orte_abort_on_non_zero_status true \
    --bind-to hwthread \
    --map-by ppr:$nb_procs_per_node:node:pe=$nb_physical_cores \
    $cmd")
    # "mpirun --hostfile $hostfile $grid5000 --mca orte_abort_on_non_zero_status false --bind-to hwthread --map-by ppr:$nb_procs_per_node:node:pe=$nb_physical_cores $cmd"
    args=$(echo -n "$args -gshr-strat=$gstrat -dist")
fi

echo $cmd $args >$metric_dir/info_${gstrat}_${lstrat}.txt

title_in_center "Using Command: ${info_theme}$cmd $args${reset_theme} on input file ${info_theme}$input_files${reset_theme}"

# Read input files line by line
# while IFS= read -r f || [[ -n "$f" ]]; do
for f in $(cat ${input_files}); do
    if [[ ! -f "$f" ]]; then
        echo -e "${error_theme}$f is not a file or doesn't exist${reset_theme}"
        continue
    fi

    echo -e "${info_theme}Using file $f${reset_theme}"

    # Getting info from the cnf file
    solution=$(head "$f" | awk 'match($0, /^c NOTE: ([a-zA-z ]*(sat|Sat|SAT)[a-zA-z ]+)/, cap) {print cap[1]}')
    nb_clauses=$(head "$f" | awk 'match($0, /^p cnf [0-9]+ ([0-9]+)/, cap) {print cap[1]}')

    echo -e "${info_theme}Nb Clause = ${nb_clauses}${reset_theme}"

    # Init outputs
    result=""
    instance=${f##*/}       # extract the file name
    instance=${instance%.*} # remove the .cnf from file name
    output_file="${metric_dir}/logs/log_${instance}.txt"
    error_file="${metric_dir}/logs/err_${instance}.txt"

    # Execute command
    start_time=$(date +%s.%N)

    # Run the command directly with timeout
    timeout $timeout $cmd $args $f 1>$output_file 2>$error_file &
    pid=$!

    # Wait for the command to finish or be killed by timeout
    wait $pid
    exit_status=$?

    end_time=$(echo "$(date +%s) + $(date +%N) / 1000000000" | bc -l)

    # Capture solution answered, even if timed out
    mapfile -t results < <(awk 'match($0, /^s ([A-Z]+)$/, cap) {print cap[1]}' "$output_file")

    # Check if the command timed out
    if [ $exit_status -eq 124 ]; then
        echo -e "${error_theme} Process interrupted due to timeout! ${reset_theme}"
        if [ ${#results[@]} -eq 0 ] || [ "${results[0]}" = "UNKNOWN" ]; then
            results=("TIMEOUT")
        else
            echo -e "${error_theme} Unexpected result at timeout: ${results[0]} ${reset_theme}"
        fi
        cleanup # Force cleanup after timeout
    fi

    end_time=$(date +%s.%N)

    # memory_used=$(awk '/Maximum Resident Set Size:/ {match($0, /([0-9]+) KB/, arr); print arr[1]}' "$output_file")
    # time_spent=$(awk 'match($0, /Resolution\s*time:\s*([0-9]+.?[0-9]*)\s*s/, cap) {print cap[1]}' "$output_file")
    # winner_family=$(awk 'match($0, /The winner is.*of family\s*([A-Z_]+)\s*.*$/, cap) {print cap[1]}' "$output_file")
    mapfile -t winner_type < <(awk 'match($0, /The winner is.*of type:\s*([A-Z_]+)\s*.*$/, cap) {print cap [1]}' "$output_file")

    # memory_used=$(bc <<<"scale=3; $memory_used / 1024")
    time_spent_bash=$(bc <<<"scale=3; $end_time - $start_time")
    case ${results[0]} in
    "UNKNOWN")
        echo -e "${error_theme} Process returned UNKNOWN! ${reset_theme}"
        mypar2=$(bc <<<"scale=3; ($timeout)*2")
        ((nbUNKNOWN++))
        ;;
    "TIMEOUT")
        # echo -e "${error_theme} Process returned TIMEOUT! ${reset_theme}"
        mypar2=$(bc <<<"scale=3; ($timeout)*2")
        ((nbTIMEOUT++))
        ;;
    "SATISFIABLE")
        mypar2=$time_spent_bash
        ((nbSAT++))
        output=$("$script_dir/SAT" "$f" "$output_file")
        last_line=$(echo "$output" | tail -n 1)
        if [[ "$last_line" == "11" ]]; then
            echo -e "${success_theme} SAT solution is correct. ${reset_theme}"
        elif [[ "$last_line" == "-1" ]]; then
            echo -e "${error_theme} SAT solution is wrong. ${reset_theme}"
            results[0]="WRONG_SATISFIABLE"
        else
            echo -e "${error_theme} SAT returned an unexpected value. ${reset_theme}"
        fi
        ;;
    "UNSATISFIABLE")
        mypar2=$time_spent_bash
        ((nbUNSAT++))
        ;;
    *)
        echo -e "${error_theme} Error occured with file $f, please check logs: \n\t$error_file\n\t$output_file ${reset_theme}"
        cat $error_file
        exit -1
        ;;
    esac

    echo -e "${log_theme}\t bash captured results: ${results[0]}${reset_theme}"
    # echo -e "${log_theme}\t bash captured time: ${time_spent} s${reset_theme}"
    # echo -e "${log_theme}\t bash captured memory use: ${memory_used} Mo ${reset_theme}"
    # echo -e "${log_theme}\t bash captured family: ${winner_family}${reset_theme}"
    echo -e "${log_theme}\t bash captured type: ${winner_type[0]}${reset_theme}"
    echo -e "${log_theme}\t Time spent by bash: $time_spent_bash s${reset_theme}"

    echo "$instance,$mypar2,${results[0]},${winner_type[0]}" >>"$timesPerIntance"

    total_time_spent=$(bc <<<"$total_time_spent + $time_spent_bash")
    total_mypar2=$(bc <<<"$total_mypar2 + $mypar2")

    echo -e "\n"
done #<"$input_files"

echo "${instance_name},$total_time_spent,$total_mypar2,$nbSAT,$nbUNSAT,$((nbSAT + nbUNSAT)),$nbTIMEOUT,$nbUNKNOWN,$((nbTIMEOUT + nbUNKNOWN))" >>$times_file

title_in_center "${info_theme}Total time spent for $1 inputs: $total_time_spent seconds${reset_theme}"
