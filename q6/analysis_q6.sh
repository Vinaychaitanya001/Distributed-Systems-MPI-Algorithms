#!/bin/bash
#SBATCH --job-name=q6_analysis
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=8
#SBATCH --time=00:15:00
#SBATCH --output=q6_analysis.log
#SBATCH --partition=debug

echo "Loading MPI module..."
module load hpcx-2.7.0/hpcx-ompi

echo "Compiling MPI Program and Generator..."
mpicxx -O3 -o q6_components q6_components.cpp
g++ -O3 -o generate_graph generate_graph.cpp

echo "Clearing old analysis file..."
> analysis.txt

SIZES=(1000 10000 50000 100000)
PROCESS_COUNTS=(1 2 4 8)

for V in "${SIZES[@]}"; do
    echo "======================================"
    echo "Generating graph with V=$V..."
    ./generate_graph $V 20
    
    for P in "${PROCESS_COUNTS[@]}"; do
        echo "Running V=$V with P=$P process(es)..."
        mpirun --bind-to none --mca coll_hcoll_enable 0 --mca pml ob1 --mca btl ^openib -n $P ./q6_components
    done
done

echo "Done! The performance data is in analysis.txt!"
