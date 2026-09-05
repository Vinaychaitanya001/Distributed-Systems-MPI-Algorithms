#!/bin/bash
#SBATCH --job-name=q6_run
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=4  
#SBATCH --time=00:05:00
#SBATCH --output=q6_run.log
#SBATCH --partition=debug

echo "Loading MPI module..."
module load hpcx-2.7.0/hpcx-ompi

echo "Compiling MPI Program..."
mpicxx -O3 -o q6_components q6_components.cpp

echo "Running Connected Components on input.txt..."
# mpirun will automatically use the 4 tasks allocated by Slurm above
mpirun --bind-to none --mca coll_hcoll_enable 0 --mca pml ob1 --mca btl ^openib ./q6_components

echo "Done! Check output.txt for the final sorted components."
