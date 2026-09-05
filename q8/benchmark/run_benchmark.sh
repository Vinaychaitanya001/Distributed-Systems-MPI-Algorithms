#!/bin/bash

set -u

# ============================================================
# Q8 Benchmark - LOCAL
# ============================================================

SEQ_EXEC="./weather_seq"
MPI_EXEC="./weather_mpi"
GENERATOR="./generate_data.py"

DATA_DIR="benchmark/data"
RESULT_DIR="benchmark/results"

mkdir -p "$DATA_DIR"
mkdir -p "$RESULT_DIR"


# ============================================================
# Configuration
# ============================================================

# Larger datasets for performance measurements.
DATASET_SIZES=(
    100000
    1000000
    5000000
)

# MPI process counts to test locally.
MPI_PROCESSES=(
    1
    2
    4
    8
)

# Repetitions for each configuration.
RUNS=5

# Generator parameters.
K=10
S=100
SEED=12345


# ============================================================
# Check required files
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
# Print configuration
# ============================================================

echo
echo "=============================================="
echo "Q8 LOCAL BENCHMARK"
echo "=============================================="
echo

echo "Datasets:"
printf '  N = %s\n' "${DATASET_SIZES[@]}"
echo

echo "MPI process counts:"
printf '  P = %s\n' "${MPI_PROCESSES[@]}"
echo

echo "Runs per configuration: $RUNS"
echo

echo "K = $K"
echo "S = $S"
echo "Seed = $SEED"
echo


# ============================================================
# Generate datasets
# ============================================================

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
# Clear previous benchmark results
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
            echo
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
        echo "  MPI processes: P=$P"

        for RUN in $(seq 1 "$RUNS"); do

            OUTPUT="$RESULT_DIR/stdout_mpi_${N}_p${P}_run${RUN}.txt"

            TIMING_FILE="$RESULT_DIR/timing_mpi_${N}_p${P}_run${RUN}.txt"

            echo "    Run $RUN/$RUNS"

            mpirun \
                -np "$P" \
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
# Create raw benchmark CSV
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

        echo "$N,seq,1,$RUN,$TOTAL,,,,," >> "$SUMMARY"

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
echo "LOCAL BENCHMARK COMPLETED"
echo "=============================================="
echo

echo "Raw results:"
echo "  $RESULT_DIR/"

echo
echo "Summary:"
echo "  $SUMMARY"

echo