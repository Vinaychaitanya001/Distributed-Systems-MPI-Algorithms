#!/bin/bash

#SBATCH --job-name=q8-weather-benchmark
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=8
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=04:00:00
#SBATCH --partition=debug
#SBATCH --output=q8_benchmark_%j.log
#SBATCH --error=q8_benchmark_%j.err


set -u


# ============================================================
# Load MPI
# ============================================================

module load hpcx-2.7.0/hpcx-ompi


# ============================================================
# Move to project directory
# ============================================================

# CHANGE THIS to your actual project directory on the RCE.
cd /home/cs3401.16/A1/problem3/newrun

if [ $? -ne 0 ]; then
    echo "ERROR: could not enter project directory"
    exit 1
fi


# ============================================================
# Configuration
# ============================================================

SEQ_EXEC="./weather_seq"
MPI_EXEC="./weather_mpi"
GENERATOR="./generate_data.py"

DATA_DIR="benchmark/data"
RESULT_DIR="benchmark/results"

mkdir -p "$DATA_DIR"
mkdir -p "$RESULT_DIR"


# ------------------------------------------------------------
# Dataset sizes
# ------------------------------------------------------------

DATASET_SIZES=(
    100000
    1000000
    5000000
)


# ------------------------------------------------------------
# MPI process counts
#
# Allocation:
#   4 nodes × 8 tasks/node = 32 MPI tasks
#
# We test powers of two.
# ------------------------------------------------------------

MPI_PROCESSES=(
    1
    2
    4
    8
    16
    32
)


# ------------------------------------------------------------
# Repetitions
# ------------------------------------------------------------

RUNS=5


# ------------------------------------------------------------
# Generator parameters
# ------------------------------------------------------------

K=10
S=100
SEED=12345


# ============================================================
# Print job information
# ============================================================

echo
echo "=============================================="
echo "Q8 RCE BENCHMARK"
echo "=============================================="
echo

echo "SLURM JOB ID:"
echo "  $SLURM_JOB_ID"

echo
echo "Allocated nodes:"
echo "  $SLURM_NNODES"

echo
echo "Total allocated tasks:"
echo "  $SLURM_NTASKS"

echo
echo "Node list:"
echo "  $SLURM_NODELIST"

echo
echo "MPI process counts:"
printf '  P = %s\n' "${MPI_PROCESSES[@]}"

echo
echo "Dataset sizes:"
printf '  N = %s\n' "${DATASET_SIZES[@]}"

echo
echo "Runs per configuration:"
echo "  $RUNS"

echo


# ============================================================
# Check executables/files
# ============================================================

if [ ! -x "$SEQ_EXEC" ]; then
    echo "ERROR: Sequential executable not found:"
    echo "       $SEQ_EXEC"
    exit 1
fi

if [ ! -x "$MPI_EXEC" ]; then
    echo "ERROR: MPI executable not found:"
    echo "       $MPI_EXEC"
    exit 1
fi

if [ ! -f "$GENERATOR" ]; then
    echo "ERROR: Data generator not found:"
    echo "       $GENERATOR"
    exit 1
fi


# ============================================================
# Compile
# ============================================================

echo
echo "=============================================="
echo "COMPILING"
echo "=============================================="

echo "Compiling sequential implementation..."

g++ -O2 -std=c++17 \
    -o "$SEQ_EXEC" \
    weather_seq.cpp

if [ $? -ne 0 ]; then
    echo "ERROR: sequential compilation failed"
    exit 1
fi


echo "Compiling MPI implementation..."

mpicxx -O2 -std=c++17 \
    -o "$MPI_EXEC" \
    weather_mpi.cpp

if [ $? -ne 0 ]; then
    echo "ERROR: MPI compilation failed"
    exit 1
fi


echo "Compilation successful."


# ============================================================
# Generate datasets
# ============================================================

echo
echo "=============================================="
echo "GENERATING DATASETS"
echo "=============================================="

for N in "${DATASET_SIZES[@]}"; do

    DATASET="$DATA_DIR/weather_${N}.txt"

    if [ -f "$DATASET" ]; then

        echo "Dataset already exists:"
        echo "  $DATASET"

    else

        echo "Generating N=$N"

        python3 "$GENERATOR" \
            --N "$N" \
            --K "$K" \
            --S "$S" \
            --seed "$SEED" \
            --output "$DATASET"

        if [ $? -ne 0 ]; then
            echo "ERROR: dataset generation failed for N=$N"
            exit 1
        fi

    fi

done


# ============================================================
# Clear old results
# ============================================================

echo
echo "=============================================="
echo "CLEARING OLD RESULTS"
echo "=============================================="

rm -f "$RESULT_DIR"/timing_*.txt
rm -f "$RESULT_DIR"/runtime_*.txt
rm -f "$RESULT_DIR"/stdout_*.txt
rm -f "$RESULT_DIR"/benchmark_summary.csv


# ============================================================
# Sequential benchmarks
# ============================================================

echo
echo "=============================================="
echo "SEQUENTIAL BENCHMARKS"
echo "=============================================="

for N in "${DATASET_SIZES[@]}"; do

    DATASET="$DATA_DIR/weather_${N}.txt"

    echo
    echo "Sequential: N=$N"

    for RUN in $(seq 1 "$RUNS"); do

        OUTPUT="$RESULT_DIR/stdout_seq_${N}_run${RUN}.txt"
        TIME_OUTPUT="$RESULT_DIR/runtime_seq_${N}_run${RUN}.txt"

        echo "  Run $RUN/$RUNS"

        /usr/bin/time \
            -f "%e" \
            -o "$TIME_OUTPUT" \
            "$SEQ_EXEC" "$DATASET" \
            > "$OUTPUT"

        STATUS=$?

        if [ $STATUS -ne 0 ]; then
            echo "ERROR: sequential benchmark failed"
            echo "       N=$N RUN=$RUN"
            exit 1
        fi

    done

done


# ============================================================
# MPI benchmarks
# ============================================================

echo
echo "=============================================="
echo "MPI BENCHMARKS"
echo "=============================================="

for N in "${DATASET_SIZES[@]}"; do

    DATASET="$DATA_DIR/weather_${N}.txt"

    echo
    echo "Dataset N=$N"

    for P in "${MPI_PROCESSES[@]}"; do

        echo
        echo "MPI processes: P=$P"

        for RUN in $(seq 1 "$RUNS"); do

            OUTPUT="$RESULT_DIR/stdout_mpi_${N}_p${P}_run${RUN}.txt"

            TIMING_FILE="$RESULT_DIR/timing_mpi_${N}_p${P}_run${RUN}.txt"

            echo "  Run $RUN/$RUNS"

            srun \
                --ntasks="$P" \
                --cpus-per-task=1 \
                "$MPI_EXEC" \
                "$DATASET" \
                "$TIMING_FILE" \
                > "$OUTPUT"

            STATUS=$?

            if [ $STATUS -ne 0 ]; then
                echo
                echo "ERROR: MPI benchmark failed"
                echo "       N=$N"
                echo "       P=$P"
                echo "       RUN=$RUN"
                exit 1
            fi

        done

    done

done


# ============================================================
# Create benchmark summary
# ============================================================

SUMMARY="$RESULT_DIR/benchmark_summary.csv"

echo
echo "=============================================="
echo "CREATING BENCHMARK SUMMARY"
echo "=============================================="

echo "N,type,processes,run,total_time,distribution_time,computation_time,aggregation_time,final_processing_time,communication_time" \
    > "$SUMMARY"


# ============================================================
# Sequential results
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
# MPI results
# ============================================================

for N in "${DATASET_SIZES[@]}"; do

    for P in "${MPI_PROCESSES[@]}"; do

        for RUN in $(seq 1 "$RUNS"); do

            TIMING_FILE="$RESULT_DIR/timing_mpi_${N}_p${P}_run${RUN}.txt"

            TOTAL=$(grep "^TIMING_TOTAL " "$TIMING_FILE" | awk '{print $2}')
            DISTRIBUTION=$(grep "^TIMING_DISTRIBUTION " "$TIMING_FILE" | awk '{print $2}')
            COMPUTATION=$(grep "^TIMING_COMPUTATION " "$TIMING_FILE" | awk '{print $2}')
            AGGREGATION=$(grep "^TIMING_AGGREGATION " "$TIMING_FILE" | awk '{print $2}')
            FINAL_PROCESSING=$(grep "^TIMING_FINAL_PROCESSING " "$TIMING_FILE" | awk '{print $2}')
            COMMUNICATION=$(grep "^TIMING_COMMUNICATION " "$TIMING_FILE" | awk '{print $2}')

            echo "$N,mpi,$P,$RUN,$TOTAL,$DISTRIBUTION,$COMPUTATION,$AGGREGATION,$FINAL_PROCESSING,$COMMUNICATION" \
                >> "$SUMMARY"

        done

    done

done


# ============================================================
# Finished
# ============================================================

echo
echo "=============================================="
echo "RCE BENCHMARK COMPLETED"
echo "=============================================="
echo

echo "Job ID:"
echo "  $SLURM_JOB_ID"

echo
echo "Results:"
echo "  $RESULT_DIR/"

echo
echo "Summary:"
echo "  $SUMMARY"

echo