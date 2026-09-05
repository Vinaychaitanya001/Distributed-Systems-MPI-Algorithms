# Q6: Connected Components of a Large Graph

## Compilation
To compile the programs on the cluster, load the required MPI module and compile both the graph generator and the MPI program with O3 optimizations:

```bash
module load hpcx-2.7.0/hpcx-ompi
g++ -O3 -o generate_graph generate_graph.cpp
mpicxx -O3 -o q6_components q6_components.cpp
```

## Dataset Generation
Because the assignment requires varying input sizes up to $V = 10^5$, an automated dataset generator `generate_graph.cpp` is provided. This uses a fixed seed to ensure reproducibility.
You can run it manually:
```bash
./generate_graph <Number of Vertices> <Max Degree>
# Example: ./generate_graph 10000 20
```
This generates a strict, undirected adjacency list in `input.txt`.

## Execution (For TA Grading)
The `q6_components` program automatically reads `input.txt` and outputs the strictly sorted components to `output.txt`. 
To run a single test on an existing `input.txt` file (e.g. for grading), you can submit the basic run script:
```bash
dos2unix submit_q6.sh
sbatch submit_q6.sh
```

## Benchmarking (For Report Analysis)
To generate the full speedup and efficiency benchmark table across $P \in \{1, 2, 4, 8\}$ and varying input sizes ($V \in \{1000, 10000, 50000, 100000\}$), submit the analysis script:
```bash
dos2unix analysis_q6.sh
sbatch analysis_q6.sh
```
This script will automatically generate the graphs, run the MPI algorithm iteratively, and output the scaling metrics to `analysis.txt`.

## Correctness Verification
The program features strict internal verification. Rank 0 runs a standard, sequential Label Propagation over the full graph to find the true Connected Components (serving as the $T_1$ baseline). It then strictly compares its sequential result with the distributed MPI result. It outputs `Status: SUCCESS` if and only if there are zero discrepancies.

## Output Format
The final result is written to `output.txt` exactly as requested by the assignment constraint:
*   Contains $V$ lines.
*   Format: `<vertex_id> <component_id>`.
*   Strictly sorted by `vertex_id` in ascending order.
*   The component ID represents the minimum vertex ID within that connected component.
