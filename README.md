# Distributed Systems: MPI Algorithms

This repository contains the MPI (Message Passing Interface) implementations for Distributed Systems Homework 2. The project focuses on distributed algorithms, parallel graph processing, and large-scale real-world data analytics.

All implementations were written in C++ and benchmarked on a SLURM high-performance computing cluster using OpenMPI.

## Project Structure

The repository is divided into three main questions, each corresponding to a different class of distributed problems:

### 1. Question 3: Distributed Bitonic Sort (`/q3`)
- **Description:** A parallel implementation of Bitonic Sort. 
- **Mechanism:** The array is distributed across $P$ processes. Each process sorts its local chunk, followed by a distributed Bitonic Merge network where pairs of processes exchange arrays, perform position-wise compare-exchanges, and keep their respective halves.
- **Scale:** Benchmarked on arrays up to $N = 131,072$ elements.
- **Highlights:** Bypassed file I/O bottlenecks by generating data programmatically. Demonstrates clear hardware bandwidth saturation limits on a single node.

### 2. Question 6: Connected Components in a Large Graph (`/q6`)
- **Description:** A parallel label propagation algorithm to find connected components in an undirected graph.
- **Mechanism:** The graph (represented as adjacency lists) is distributed across processes. Vertices initialize their component ID to their own Vertex ID. In each iteration, processes exchange their component IDs with neighbors. The global state is synchronized using `MPI_Allreduce` until convergence (no IDs change).
- **Scale:** Benchmarked on massive graphs up to $10^5$ vertices and $10^6$ edges.
- **Highlights:** Includes a custom C++ dataset generator and detailed mathematical analysis on the communication vs. computation bottleneck.

### 3. Question 8: Large-Scale Weather Data Analytics (`/q8`)
- **Description:** A real-world analytics pipeline for processing massive weather telemetry logs.
- **Mechanism:** Parses millions of logs containing timestamps, station IDs, temperature, humidity, and wind speed. Distributed workers compute local maximums, minimums, and averages, which are then globally reduced to find the hottest/coldest measurements and Top-K busiest stations.
- **Scale:** Benchmarked on datasets ranging from $10,000$ to $1,000,000$ telemetry logs.
- **Highlights:** explicitly tested across 4 physical cluster nodes (using 32 allocated cores) to map pure cross-node network communication speeds.

## Execution and Benchmarking

Each subdirectory contains its own standalone codebase, SLURM batch scripts, and LaTeX reports:
- `submit_*.sh`: Single-run scripts designed for TA grading and correctness verification.
- `analysis_*.sh`: Comprehensive benchmark suites that loop over varying input sizes ($N$) and process counts ($P = 1, 2, 4, 8$).
- `plot_analysis.py`: Python scripts to parse the SLURM benchmark logs and generate Speedup and Efficiency graphs.

## Requirements
- C++17 Compiler
- OpenMPI (e.g., `hpcx-2.7.0/hpcx-ompi`)
- SLURM Workload Manager (for running batch scripts)
