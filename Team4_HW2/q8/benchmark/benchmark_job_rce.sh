#!/bin/bash

#SBATCH --job-name=q8-benchmark
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=8
#SBATCH --cpus-per-task=1
#SBATCH --time=02:00:00
#SBATCH --partition=debug
#SBATCH --output=benchmark_%j.log
#SBATCH --error=benchmark_%j.err

set -e

# ============================================================
# PROJECT DIRECTORY
# ============================================================

PROJECT_DIR="/home/cs3401.04/q8"

cd "$PROJECT_DIR"

# ============================================================
# CONFIGURATION
# ============================================================

DATASET_SIZES=(
    10000
    100000
    1000000
)

MPI_PROCESSES=(
    1
    2
    4
    8
)

RUNS=5

K=10
S=100
SEED=12345

DATA_DIR="benchmark/data"
RESULT_DIR="benchmark/results"

SEQ_EXEC="./weather_seq"
MPI_EXEC="./weather_mpi"
GENERATOR="./generate_data.py"


# ============================================================
# HEADER
# ============================================================

echo
echo "=============================================="
echo "Q8 RCE BENCHMARK"
echo "=============================================="
echo

echo "SLURM JOB ID:"
echo "$SLURM_JOB_ID"

echo
echo "SLURM NODE LIST:"
echo "$SLURM_NODELIST"

echo
echo "SLURM NODES:"
echo "$SLURM_NNODES"

echo
echo "SLURM TASKS:"
echo "$SLURM_NTASKS"

echo
echo "TASKS PER NODE:"
echo "$SLURM_NTASKS_PER_NODE"


# ============================================================
# LOAD HPC-X / OPENMPI
# ============================================================

echo
echo "=============================================="
echo "LOADING OPENMPI"
echo "=============================================="
echo

module load hpcx-2.7.0/hpcx-ompi

echo "mpicxx = $(which mpicxx)"
echo "mpirun = $(which mpirun)"
echo


# ============================================================
# GET ACTUAL ALLOCATED NODES
# ============================================================

echo
echo "=============================================="
echo "ALLOCATED NODES"
echo "=============================================="
echo

mapfile -t NODES < <(
    scontrol show hostnames "$SLURM_NODELIST"
)

NUM_NODES="${#NODES[@]}"

echo "Actual nodes allocated:"
for NODE in "${NODES[@]}"; do
    echo "  $NODE"
done

echo
echo "Number of nodes: $NUM_NODES"


# ============================================================
# CREATE MPI HOST LIST
# ============================================================

MPI_HOSTS=""

for NODE in "${NODES[@]}"; do

    if [ -z "$MPI_HOSTS" ]; then
        MPI_HOSTS="$NODE"
    else
        MPI_HOSTS="$MPI_HOSTS,$NODE"
    fi

done

echo
echo "MPI host list:"
echo "$MPI_HOSTS"


# ============================================================
# NODE TEST
# ============================================================

echo
echo "=============================================="
echo "NODE TEST"
echo "=============================================="
echo

echo "Testing allocated nodes..."

srun \
    --nodes="$NUM_NODES" \
    --ntasks="$NUM_NODES" \
    --ntasks-per-node=1 \
    hostname

echo
echo "Node test successful."


# ============================================================
# CHECK PROJECT FILES
# ============================================================

echo
echo "=============================================="
echo "CHECKING PROJECT FILES"
echo "=============================================="
echo

REQUIRED_FILES=(
    "weather_seq.cpp"
    "weather_mpi.cpp"
    "generate_data.py"
)

for FILE in "${REQUIRED_FILES[@]}"; do

    if [ ! -f "$FILE" ]; then
        echo "ERROR: Required file not found:"
        echo "       $PROJECT_DIR/$FILE"
        exit 1
    fi

done

echo "All required project files found."


# ============================================================
# COMPILATION
# ============================================================

echo
echo "=============================================="
echo "COMPILATION"
echo "=============================================="
echo

echo "Compiling sequential version..."

g++ -O2 -std=c++17 \
    weather_seq.cpp \
    -o weather_seq

echo "Compiling MPI version..."

mpicxx -O2 -std=c++17 \
    weather_mpi.cpp \
    -o weather_mpi

echo
echo "Compilation successful."


# ============================================================
# DATA DIRECTORIES
# ============================================================

mkdir -p "$DATA_DIR"
mkdir -p "$RESULT_DIR"


# ============================================================
# GENERATE DATASETS
# ============================================================

echo
echo "=============================================="
echo "GENERATING BENCHMARK DATASETS"
echo "=============================================="
echo

for N in "${DATASET_SIZES[@]}"; do

    DATASET="$DATA_DIR/weather_${N}.txt"

    echo
    echo "Dataset N=$N"

    python3 "$GENERATOR" \
        --N "$N" \
        --K "$K" \
        --S "$S" \
        --seed "$SEED" \
        --output "$DATASET"

done


# ============================================================
# CLEAR OLD RESULTS
# ============================================================

echo
echo "=============================================="
echo "CLEARING OLD RESULTS"
echo "=============================================="
echo

rm -f "$RESULT_DIR"/*.txt
rm -f "$RESULT_DIR"/*.csv

echo "Old benchmark results removed."


# ============================================================
# SEQUENTIAL BENCHMARKS
# ============================================================

echo
echo "=============================================="
echo "SEQUENTIAL BENCHMARKS"
echo "=============================================="
echo

for N in "${DATASET_SIZES[@]}"; do

    DATASET="$DATA_DIR/weather_${N}.txt"

    echo
    echo "Dataset N=$N"

    for RUN in $(seq 1 "$RUNS"); do

        echo "  Run $RUN/$RUNS"

        OUTPUT="$RESULT_DIR/stdout_seq_${N}_run${RUN}.txt"
        TIME_OUTPUT="$RESULT_DIR/runtime_seq_${N}_run${RUN}.txt"

        /usr/bin/time \
            -f "%e" \
            -o "$TIME_OUTPUT" \
            "$SEQ_EXEC" \
            "$DATASET" \
            > "$OUTPUT"

    done

done


# ============================================================
# MPI COMMUNICATION TEST
# ============================================================

echo
echo "=============================================="
echo "MPI COMMUNICATION TEST"
echo "=============================================="
echo

echo "Testing MPI across allocated nodes..."

# Use one process per allocated node.
# Explicit host list avoids OpenMPI's broken Slurm
# topology detection on this RCE installation.

"$MPI_EXEC" >/dev/null 2>&1 || true

mpirun \
    --host "$MPI_HOSTS" \
    -np "$NUM_NODES" \
    --map-by slot \
    --bind-to none \
    --mca plm rsh \
    --mca pml ob1 \
    --mca btl self,vader,tcp \
    --mca btl_tcp_if_include eno8303 \
    hostname \
    > "$RESULT_DIR/mpi_node_test.txt"

echo
echo "MPI communication test output:"
cat "$RESULT_DIR/mpi_node_test.txt"

echo
echo "MPI communication test completed."


# ============================================================
# MPI BENCHMARKS
# ============================================================

echo
echo "=============================================="
echo "MPI BENCHMARKS"
echo "=============================================="
echo

for N in "${DATASET_SIZES[@]}"; do

    DATASET="$DATA_DIR/weather_${N}.txt"

    echo
    echo "=============================================="
    echo "Dataset N=$N"
    echo "=============================================="

    for P in "${MPI_PROCESSES[@]}"; do

        echo
        echo "----------------------------------------------"
        echo "MPI processes: $P"
        echo "----------------------------------------------"

        # ----------------------------------------------------
        # Determine whether P fits in the allocation
        # ----------------------------------------------------

        MAX_PROCESSES=$SLURM_NTASKS

        if [ "$P" -gt "$MAX_PROCESSES" ]; then
            echo "Skipping P=$P"
            echo "Allocation only provides $MAX_PROCESSES tasks."
            continue
        fi


        # ----------------------------------------------------
        # Select hosts for this process count
        #
        # For P <= number of nodes:
        #   one MPI process per node
        #
        # For P > number of nodes:
        #   distribute processes across allocated nodes
        # ----------------------------------------------------

        if [ "$P" -le "$NUM_NODES" ]; then

            HOSTS_FOR_P=""

            for ((i=0; i<P; i++)); do

                if [ -z "$HOSTS_FOR_P" ]; then
                    HOSTS_FOR_P="${NODES[$i]}:1"
                else
                    HOSTS_FOR_P="$HOSTS_FOR_P,${NODES[$i]}:1"
                fi

            done

        else

            HOSTS_FOR_P=""

            BASE=$((P / NUM_NODES))
            REM=$((P % NUM_NODES))

            for ((i=0; i<NUM_NODES; i++)); do

                SLOTS=$BASE

                if [ "$i" -lt "$REM" ]; then
                    SLOTS=$((SLOTS + 1))
                fi

                if [ "$SLOTS" -gt 0 ]; then

                    if [ -z "$HOSTS_FOR_P" ]; then
                        HOSTS_FOR_P="${NODES[$i]}:$SLOTS"
                    else
                        HOSTS_FOR_P="$HOSTS_FOR_P,${NODES[$i]}:$SLOTS"
                    fi

                fi

            done

        fi


        echo "Hosts:"
        echo "$HOSTS_FOR_P"


        # ----------------------------------------------------
        # Five measured runs
        # ----------------------------------------------------

        for RUN in $(seq 1 "$RUNS"); do

            echo "  Run $RUN/$RUNS"

            OUTPUT="$RESULT_DIR/stdout_mpi_${N}_p${P}_run${RUN}.txt"

            TIMING_FILE="$RESULT_DIR/timing_mpi_${N}_p${P}_run${RUN}.txt"


            mpirun \
                --host "$HOSTS_FOR_P" \
                -np "$P" \
                --map-by slot \
                --bind-to none \
                --mca plm rsh \
                --mca pml ob1 \
                --mca btl self,vader,tcp \
                --mca btl_tcp_if_include eno8303 \
                "$MPI_EXEC" \
                "$DATASET" \
                "$TIMING_FILE" \
                > "$OUTPUT"


            if [ $? -ne 0 ]; then

                echo
                echo "ERROR: MPI benchmark failed."
                echo "N=$N"
                echo "P=$P"
                echo "RUN=$RUN"

                exit 1

            fi

        done

    done

done


# ============================================================
# CREATE SUMMARY CSV
# ============================================================

echo
echo "=============================================="
echo "CREATING BENCHMARK SUMMARY"
echo "=============================================="
echo

SUMMARY="$RESULT_DIR/benchmark_summary.csv"

echo "N,type,processes,run,total_time,distribution_time,computation_time,aggregation_time,final_processing_time,communication_time" \
    > "$SUMMARY"


# ============================================================
# SEQUENTIAL RESULTS
# ============================================================

for N in "${DATASET_SIZES[@]}"; do

    for RUN in $(seq 1 "$RUNS"); do

        TIME_FILE="$RESULT_DIR/runtime_seq_${N}_run${RUN}.txt"

        TOTAL=$(cat "$TIME_FILE")

        echo "$N,seq,1,$RUN,$TOTAL,,,,," \
            >> "$SUMMARY"

    done

done


# ============================================================
# MPI RESULTS
# ============================================================

for N in "${DATASET_SIZES[@]}"; do

    for P in "${MPI_PROCESSES[@]}"; do

        for RUN in $(seq 1 "$RUNS"); do

            TIMING_FILE="$RESULT_DIR/timing_mpi_${N}_p${P}_run${RUN}.txt"

            if [ ! -f "$TIMING_FILE" ]; then
                continue
            fi

            TOTAL=$(grep "^TIMING_TOTAL " \
                "$TIMING_FILE" | awk '{print $2}')

            DISTRIBUTION=$(grep "^TIMING_DISTRIBUTION " \
                "$TIMING_FILE" | awk '{print $2}')

            COMPUTATION=$(grep "^TIMING_COMPUTATION " \
                "$TIMING_FILE" | awk '{print $2}')

            AGGREGATION=$(grep "^TIMING_AGGREGATION " \
                "$TIMING_FILE" | awk '{print $2}')

            FINAL_PROCESSING=$(grep "^TIMING_FINAL_PROCESSING " \
                "$TIMING_FILE" | awk '{print $2}')

            COMMUNICATION=$(grep "^TIMING_COMMUNICATION " \
                "$TIMING_FILE" | awk '{print $2}')


            echo "$N,mpi,$P,$RUN,$TOTAL,$DISTRIBUTION,$COMPUTATION,$AGGREGATION,$FINAL_PROCESSING,$COMMUNICATION" \
                >> "$SUMMARY"

        done

    done

done


# ============================================================
# COMPLETE
# ============================================================

echo
echo "=============================================="
echo "BENCHMARK COMPLETE"
echo "=============================================="
echo

echo "Job ID:"
echo "$SLURM_JOB_ID"

echo
echo "Allocated nodes:"
for NODE in "${NODES[@]}"; do
    echo "  $NODE"
done

echo
echo "Results:"
echo "  $RESULT_DIR/"

echo
echo "Summary:"
echo "  $SUMMARY"

echo
echo "Finished successfully."


