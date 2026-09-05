#!/bin/bash
#SBATCH --job-name=q3_bitonic
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=8
#SBATCH --time=00:10:00
#SBATCH --output=bitonic_results.log
#SBATCH --partition=debug

echo "Loading MPI module..."
module load hpcx-2.7.0/hpcx-ompi

echo "Compiling Bitonic Sort & Generator..."
g++ -O3 -o generate_array generate_array.cpp
mpicxx -O3 -o bitonic bitonic_sort.cpp

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "Compilation successful. Running benchmarks..."
echo "============================================="

# Define input sizes to test
SIZES=(1024 16384 65536 131072)
PROCESS_COUNTS=(1 2 4 8)

for N in "${SIZES[@]}"; do
    echo "---------------------------------------------"
    echo "Testing Input Size N = $N"
    echo "Generating input.txt..."
    ./generate_array $N
    echo "---------------------------------------------"
    for P in "${PROCESS_COUNTS[@]}"; do
        # Run with P processes (with flags to fix cluster networking bugs)
        mpirun --bind-to none --mca coll_hcoll_enable 0 --mca pml ob1 --mca btl ^openib -n $P ./bitonic
    done
done

echo "============================================="
echo "All tests complete!"
