# Question 6: Performance Analysis

## Speedup and Efficiency Plots
*(Insert your generated `speedup_plot.png` and `efficiency_plot.png` here)*

## Analysis of Communication vs. Computation

In the Parallel Label Propagation algorithm, the workload is divided into two distinct phases: 
1. **Computation**: Iterating through local adjacency lists to find the minimum known component ID for each edge.
2. **Communication**: Using `MPI_Allreduce` to synchronize the absolute minimum component ID for every vertex across all processes globally.

Based on the generated plots, we observe a textbook example of a **Communication Bottleneck**:

* **Initial Speedup ($P=2$):** For the larger graphs ($V=50,000$ and $V=100,000$), dividing the computation in half yields a noticeable speedup (peaking around 1.5x). The computational gains of splitting the edge traversals outweigh the synchronization costs between just two processes.
* **The Plateau ($P=4$ and $P=8$):** As we scale to 4 and 8 processes, the Speedup completely flatlines, and the Efficiency drops significantly (down to ~0.1 - 0.2). 
* **Why this happens:** A sequential CPU can traverse 100,000 vertices in mere milliseconds. However, the `MPI_Allreduce` step forces all $P$ processes to globally synchronize a large array of integers at the end of *every single iteration*. At $P=8$, the overhead of network/memory synchronization drastically exceeds the tiny microsecond gains of parallel edge traversal. 
* **Conclusion:** For this parallel algorithm to achieve linear scaling up to 8 nodes, the input graph would need to be orders of magnitude larger (e.g., $V = 10^8$ or $10^9$) so that the `Computation` phase takes seconds/minutes, thereby dwarfing the `Communication` phase overhead.
