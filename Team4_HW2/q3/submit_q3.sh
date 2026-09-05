#!/bin/bash
#SBATCH --job-name=q3_run
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=4  # <-- TA: CHANGE THIS NUMBER TO TEST DIFFERENT P (1, 2, 4, 8)
#SBATCH --time=00:05:00
#SBATCH --output=q3_run.log
#SBATCH --partition=debug

echo "Loading MPI module..."
module load hpcx-2.7.0/hpcx-ompi

echo "Compiling..."
g++ -O3 -o generate_array generate_array.cpp
mpicxx -O3 -o bitonic bitonic_sort.cpp

# Define the array size (TA can change this to 1024, 65536, etc.)
N=65536

echo "Generating input.txt with N=$N..."
./generate_array $N

echo "Running Bitonic Sort on input.txt..."
# mpirun will automatically use the tasks allocated by Slurm above
mpirun --bind-to none --mca coll_hcoll_enable 0 --mca pml ob1 --mca btl ^openib ./bitonic

echo "Done! Check q3_run.log for the results."
