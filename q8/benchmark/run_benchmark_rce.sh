#!/bin/bash

# ============================================================
# Q8 RCE BENCHMARK SUBMITTER
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_DIR" || exit 1

echo
echo "=============================================="
echo "Q8 RCE BENCHMARK"
echo "=============================================="
echo
echo "Submitting Slurm benchmark job..."
echo

JOB_ID=$(sbatch --parsable "$SCRIPT_DIR/benchmark_job_rce.sh")

if [ $? -ne 0 ]; then
    echo
    echo "ERROR: Failed to submit Slurm job."
    exit 1
fi

echo
echo "Submitted Slurm job: $JOB_ID"
echo
echo "Check status with:"
echo "  squeue -j $JOB_ID"
echo
echo "View output with:"
echo "  cat benchmark_${JOB_ID}.log"
echo
echo "View errors with:"
echo "  cat benchmark_${JOB_ID}.err"
echo
echo "After completion:"
echo "  ls benchmark/results/"
echo
