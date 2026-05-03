#!/bin/bash
# Extract NUMA_NODES
NUMA_NODES=$(lscpu | grep -Po 'NUMA node\(s\):\s*\K[0-9]+')
if [[ -z "$NUMA_NODES" ]]; then
    echo "Error: Could not determine NUMA_NODES."
    exit 1
fi
NUMA_NODE0_CPUS=$(lscpu | grep -Po 'NUMA node0 CPU\(s\):\s*\K[0-9,]+')
if [[ -z "$NUMA_NODE0_CPUS" ]]; then
    echo "Error: Could not determine CORES_PER_NODE."
    exit 1
fi
CORES_PER_NODE=$(echo "$NUMA_NODE0_CPUS" | tr ',' ' ' | wc -w)
echo "NUMA_NODES=$NUMA_NODES"
echo "CORES_PER_NODE=$CORES_PER_NODE"

# ----- Arguments -----
# Usage: ./script.sh <params.sh> <topology.json> <input_list> <run_name> [threads_per_process]
PARAMS_FILE="$1"
TOPOLOGY_FILE="$2"
INPUT_FILE="$3"
RUN_NAME="$4"
THREADS_PER_PROCESS="${5:-1}"

if [[ -z "$PARAMS_FILE" || -z "$TOPOLOGY_FILE" || -z "$INPUT_FILE" || -z "$RUN_NAME" ]]; then
    echo "Usage: $0 <params.sh> <topology.json> <input_list> <run_name> [threads_per_process]"
    exit 1
fi
if (( THREADS_PER_PROCESS < 1 )); then
    echo "Error: THREADS_PER_PROCESS must be >= 1"
    exit 1
fi

USAGE_PERCENTAGE=90
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# ----- Core / process layout -----
CORES_TO_USE_PER_NODE=$((CORES_PER_NODE * USAGE_PERCENTAGE / 100))
# Round down to a multiple of THREADS_PER_PROCESS
CORES_TO_USE_PER_NODE=$(( (CORES_TO_USE_PER_NODE / THREADS_PER_PROCESS) * THREADS_PER_PROCESS ))
if (( CORES_TO_USE_PER_NODE < THREADS_PER_PROCESS )); then
    echo "Error: Not enough cores per node ($CORES_PER_NODE) for THREADS_PER_PROCESS=$THREADS_PER_PROCESS at ${USAGE_PERCENTAGE}%."
    exit 1
fi
PROCS_PER_NODE=$(( CORES_TO_USE_PER_NODE / THREADS_PER_PROCESS ))
TOTAL_PROCS=$(( PROCS_PER_NODE * NUMA_NODES ))
TOTAL_CORES_TO_USE=$(( CORES_TO_USE_PER_NODE * NUMA_NODES ))

# ----- Input file list (sorted by size) -----
declare -a SORTED_FILES
readarray -t SORTED_FILES < <(ls -S $(cat "$INPUT_FILE") | sort)
TOTAL_FILES=${#SORTED_FILES[@]}

echo "Threads per process: $THREADS_PER_PROCESS"
echo "Cores per node in use: $CORES_TO_USE_PER_NODE  (total cores: $TOTAL_CORES_TO_USE)"
echo "Processes per node: $PROCS_PER_NODE  (total processes: $TOTAL_PROCS)"
echo "Total input files: $TOTAL_FILES"

OUTDIR="./outputs_${RUN_NAME}_${TIMESTAMP}"
mkdir -p "$OUTDIR"

# ----- Launch processes on each NUMA node -----
for node in $(seq 0 $((NUMA_NODES-1))); do
    NODE_CORES=($(lscpu -p | grep ",${node},," | cut -d',' -f 1))
    AVAILABLE_CORES=("${NODE_CORES[@]:0:$CORES_TO_USE_PER_NODE}")

    echo "Node $node available cores (${#AVAILABLE_CORES[@]}): ${AVAILABLE_CORES[*]}"

    # Slice node's cores into chunks of THREADS_PER_PROCESS
    for ((p=0; p<PROCS_PER_NODE; p++)); do
        start=$(( p * THREADS_PER_PROCESS ))
        PROCESS_CORES=("${AVAILABLE_CORES[@]:$start:$THREADS_PER_PROCESS}")
        CORE_LIST=$(IFS=,; echo "${PROCESS_CORES[*]}")

        GLOBAL_PROC_ID=$(( node * PROCS_PER_NODE + p ))

        # Per-process input list (round-robin across processes)
        TEMP_INPUT_FILE="${OUTDIR}/input_proc_${GLOBAL_PROC_ID}.txt"
        > "$TEMP_INPUT_FILE"
        FILE_INDEX=$GLOBAL_PROC_ID
        while (( FILE_INDEX < TOTAL_FILES )); do
            echo "${SORTED_FILES[$FILE_INDEX]}" >> "$TEMP_INPUT_FILE"
            FILE_INDEX=$(( FILE_INDEX + TOTAL_PROCS ))
        done

        if [ ! -s "$TEMP_INPUT_FILE" ]; then
            echo "No files assigned to process $GLOBAL_PROC_ID, skipping"
            continue
        fi

        # Per-process run name so outputs from launch.sh don't collide
        PROC_RUN_NAME="${RUN_NAME}_p${GLOBAL_PROC_ID}"
        OUTPUT_FILE="${OUTDIR}/output_${GLOBAL_PROC_ID}_${TIMESTAMP}.txt"

        echo "Launching process $GLOBAL_PROC_ID on node $node, cores $CORE_LIST  (run: $PROC_RUN_NAME)"

        numactl --membind=$node --physcpubind=$CORE_LIST \
            bash ./scripts/bin/launch.sh \
                "$PARAMS_FILE" \
                "$TOPOLOGY_FILE" \
                "$TEMP_INPUT_FILE" \
                "$PROC_RUN_NAME" \
            > "$OUTPUT_FILE" 2>&1 &

        sleep 0.1
    done
done

echo "All processes launched. Outputs will be available in $OUTDIR/"
echo "To monitor progress, use: tail -f ${OUTDIR}/output_*_${TIMESTAMP}.txt"
