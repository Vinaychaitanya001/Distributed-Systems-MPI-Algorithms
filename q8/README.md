# Q8 — Weather Data Processing using MPI

## 1. Problem Statement

Implement a sequential and MPI-based solution for processing a weather
measurement dataset. The program processes measurements belonging to
different weather stations and produces the required station-level results.

The MPI implementation parallelizes the processing while preserving the
correctness of the sequential solution.

---

## 2. Files

The main project files are:

```text
q8/
├── weather_seq.cpp
├── weather_mpi.cpp
├── weather_seq
├── weather_mpi
├── generate_data.py
└── benchmark/
    ├── benchmark_job_rce.sh
    ├── run_benchmark_rce.sh
    ├── analyze_results.py
    ├── check_correctness.py
    ├── data/
    └── results/
        ├── benchmark_summary.csv
        ├── processed/
        └── plots/
```

`weather_seq.cpp` contains the sequential implementation and
`weather_mpi.cpp` contains the MPI implementation.

---

## 3. Compilation

The sequential implementation can be compiled using:

```bash
g++ -O2 -std=c++17 weather_seq.cpp -o weather_seq
```

The MPI implementation can be compiled using:

```bash
mpicxx -O2 -std=c++17 weather_mpi.cpp -o weather_mpi
```

---

## 4. Input Data

The benchmark datasets are generated using the provided Python data
generation script.

The benchmark uses:

- N = 10,000 measurements
- N = 100,000 measurements
- N = 1,000,000 measurements
- 100 stations
- Top-K = 10
- Seed = 12345

The generated files are stored under:

```text
benchmark/data/
```

The benchmark data is generated using the required command-line arguments
for the provided `generate_data.py` script.

---

## 5. Running the Sequential Version

Example:

```bash
./weather_seq     benchmark/data/weather_10000.txt     timing_seq.txt
```

The output contains the required weather-processing results and timing
information.

---

## 6. Running the MPI Version

Example:

```bash
mpirun -np 4 ./weather_mpi     benchmark/data/weather_10000.txt     timing_mpi.txt
```

The MPI program reports timing information for the different phases of the
parallel computation, including distribution, computation, aggregation,
final processing and communication.

---

# 7. Correctness Verification

The MPI implementation was checked against the sequential implementation.

For each tested dataset and MPI process configuration, the final MPI output
was compared with the corresponding sequential output.

The correctness checking script is:

```text
benchmark/check_correctness.py
```

The comparison verifies that the parallel implementation produces the same
required station-level results as the sequential reference.

All tested MPI configurations passed the correctness verification.

---

# 8. Benchmark Methodology

The benchmark was executed on the RCE cluster using Slurm.

A Slurm batch job was used so that the complete benchmark could run on
allocated compute nodes without requiring manual interaction with the
allocation.

The benchmark used four allocated compute nodes. The actual node names were
determined dynamically by Slurm.

For every dataset:

- The sequential implementation was run 5 times.
- Each MPI configuration was run 5 times.
- P = 1, 2, 4 and 8 were tested on the RCE cluster.
- The median of the five runs was used as the representative runtime.

The benchmark records:

- Total runtime
- Distribution time
- Computation time
- Aggregation time
- Final processing time
- Communication time

The raw benchmark results are stored in:

```text
benchmark/results/benchmark_summary.csv
```

Processed results are stored in:

```text
benchmark/results/processed/
```

---

# 9. Benchmark Results

## 9.1 Runtime, Speedup and Efficiency

Speedup is calculated using the sequential implementation as the reference:

$$
S(P) = \frac{T_{seq}}{T_{mpi}(P)}
$$

Parallel efficiency is calculated as:

$$
E(P) = \frac{S(P)}{P}
$$

The following table contains the measured median runtimes, speedups and
efficiencies:

| Input Size (N) | P | Sequential Time (s) | MPI Time (s) | Speedup | Efficiency |
|---:|---:|---:|---:|---:|---:|
| 10,000 | 1 | 0.010000 | 0.001430 | 6.994 | 699.42% |
| 10,000 | 2 | 0.010000 | 0.009299 | 1.075 | 53.77% |
| 10,000 | 4 | 0.010000 | 0.001754 | 5.700 | 142.49% |
| 10,000 | 8 | 0.010000 | 0.002556 | 3.912 | 48.90% |
| 100,000 | 1 | 0.140000 | 0.005540 | 25.271 | 2527.12% |
| 100,000 | 2 | 0.140000 | 0.005277 | 26.528 | 1326.42% |
| 100,000 | 4 | 0.140000 | 0.008416 | 16.634 | 415.86% |
| 100,000 | 8 | 0.140000 | 0.009549 | 14.662 | 183.27% |
| 1,000,000 | 1 | 1.450000 | 0.037909 | 38.250 | 3824.97% |
| 1,000,000 | 2 | 1.450000 | 0.032352 | 44.819 | 2240.97% |
| 1,000,000 | 4 | 1.450000 | 0.055605 | 26.077 | 651.92% |
| 1,000,000 | 8 | 1.450000 | 0.074198 | 19.542 | 244.28% |

The processed speedup and efficiency data is also available in:

```text
benchmark/results/processed/speedup_efficiency.csv
```

---

## 9.2 Best MPI Configuration

The fastest tested MPI configuration for each dataset is:

| Input Size (N) | Best P | Runtime (s) | Speedup | Efficiency |
|---:|---:|---:|---:|---:|
| 10,000 | 1 | 0.001430 | 6.994 | 699.42% |
| 100,000 | 2 | 0.005277 | 26.528 | 1326.42% |
| 1,000,000 | 2 | 0.032352 | 44.819 | 2240.97% |

The complete table is available in:

```text
benchmark/results/processed/best_process_count.csv
```

---

# 10. Performance Analysis

## 10.1 Runtime vs Number of Processes

The runtime plots show how MPI execution time changes as the number of
processes increases.

For small inputs, MPI overhead represents a relatively large portion of the
execution time. Therefore, increasing the number of processes does not
necessarily improve the total runtime.

For larger inputs, the computation has more work that can be distributed
among processes. However, after a certain point, communication and
aggregation overhead become increasingly significant.

The plots are available under:

```text
benchmark/results/plots/
```

Relevant plots:

```text
runtime_vs_processes_N10000.png
runtime_vs_processes_N100000.png
runtime_vs_processes_N1000000.png
```

---

## 10.2 Speedup

Speedup is expected to increase as more processes are used when the workload
is sufficiently large and parallel overhead remains small.

The measured results show that the improvement is not linear for all
process counts. Increasing the process count eventually introduces enough
communication and aggregation overhead to reduce the benefit of additional
processes.

The speedup plots are:

```text
speedup_vs_processes_N10000.png
speedup_vs_processes_N100000.png
speedup_vs_processes_N1000000.png
speedup_vs_input_size.png
```

---

## 10.3 Parallel Efficiency

Parallel efficiency measures how effectively the available processes are
being used:

$$
E(P) = \frac{S(P)}{P}
$$

The efficiency decreases when the additional processes do not provide a
proportional reduction in runtime.

The efficiency plots are:

```text
efficiency_vs_processes_N10000.png
efficiency_vs_processes_N100000.png
efficiency_vs_processes_N1000000.png
```

The measured efficiency values greater than 100% for some configurations
should not be interpreted as conventional super-linear parallel scaling.
They arise because the measured MPI execution path is substantially faster
than the sequential implementation used as the reference, particularly for
the very small measured runtimes. Thus, these values should be interpreted
along with the runtime and phase-timing results rather than in isolation.

---

## 10.4 Effect of Input Size

Increasing the input size provides more computation that can be distributed
among MPI processes.

The sequential runtime increases with input size, while the MPI
implementation can distribute the additional work across processes.

The runtime-versus-input-size plot is:

```text
runtime_vs_input_size.png
```

The speedup-versus-input-size plot is:

```text
speedup_vs_input_size.png
```

These plots show how the usefulness of parallel execution changes as the
amount of input work increases.

---

# 11. MPI Phase Analysis

The MPI implementation records timing for several execution phases:

- Distribution
- Computation
- Aggregation
- Final processing
- Communication

The processed phase data is stored in:

```text
benchmark/results/processed/mpi_phase_times.csv
```

---

## 11.1 Computation Time

Increasing the number of MPI processes generally reduces the amount of
computation performed by each process.

This demonstrates the benefit of distributing the input workload across
multiple processes.

However, reducing computation time alone does not guarantee a reduction in
total execution time.

---

## 11.2 Communication and Aggregation Overhead

As the number of processes increases, MPI communication and result
aggregation can become more expensive.

For larger process counts, the time spent communicating and combining
results can offset the reduction in computation time.

The phase timing plots are:

```text
mpi_phase_times_N10000.png
mpi_phase_times_N100000.png
mpi_phase_times_N1000000.png
```

The percentage breakdown plots are:

```text
mpi_phase_percentage_N10000.png
mpi_phase_percentage_N100000.png
mpi_phase_percentage_N1000000.png
```

These plots help identify whether execution time is dominated by computation
or by parallel overhead.

---

# 12. Scalability

The benchmark demonstrates that the MPI implementation does not scale
linearly with the number of processes for all tested inputs.

For sufficiently large input sizes, increasing the number of processes
initially reduces computation time. However, communication and aggregation
overhead increase as more processes participate.

Consequently, there is a point beyond which adding processes increases total
runtime instead of decreasing it.

The best process count therefore depends on the input size and the relative
cost of computation and communication.

---

# 13. RCE / Slurm Execution

The benchmark is designed to run through a Slurm batch job on the RCE
cluster.

The main submission script is:

```text
benchmark/run_benchmark_rce.sh
```

It submits:

```text
benchmark/benchmark_job_rce.sh
```

The batch job handles:

1. Slurm resource allocation.
2. Loading the required MPI environment.
3. Compilation.
4. Dataset generation.
5. Sequential benchmarking.
6. MPI benchmarking.
7. Timing collection.
8. Result generation.

To submit the benchmark:

```bash
./benchmark/run_benchmark_rce.sh
```

After submission, Slurm provides a job ID. The job output and error files can
be inspected using:

```bash
cat benchmark_<JOB_ID>.log
cat benchmark_<JOB_ID>.err
```

The benchmark dynamically uses the nodes assigned by Slurm rather than
assuming fixed node names.

---

# 14. Output Files

The main benchmark outputs are:

```text
benchmark/results/
├── benchmark_summary.csv
├── processed/
│   ├── median_results.csv
│   ├── speedup_efficiency.csv
│   ├── mpi_phase_times.csv
│   └── best_process_count.csv
└── plots/
    ├── runtime_vs_processes_N10000.png
    ├── runtime_vs_processes_N100000.png
    ├── runtime_vs_processes_N1000000.png
    ├── speedup_vs_processes_N10000.png
    ├── speedup_vs_processes_N100000.png
    ├── speedup_vs_processes_N1000000.png
    ├── efficiency_vs_processes_N10000.png
    ├── efficiency_vs_processes_N100000.png
    ├── efficiency_vs_processes_N1000000.png
    ├── runtime_vs_input_size.png
    ├── speedup_vs_input_size.png
    ├── mpi_phase_times_N10000.png
    ├── mpi_phase_times_N100000.png
    ├── mpi_phase_times_N1000000.png
    ├── mpi_phase_percentage_N10000.png
    ├── mpi_phase_percentage_N100000.png
    └── mpi_phase_percentage_N1000000.png
```

---

# 15. Summary

The Q8 implementation provides both sequential and MPI versions of the
weather-data processing program.

The MPI implementation was verified against the sequential implementation
for correctness and benchmarked on the RCE cluster using multiple input
sizes and process counts.

The benchmark demonstrates the expected trade-off between parallel
computation and communication overhead. Increasing the number of processes
can reduce computation time, but communication and aggregation overhead
limit scalability.

The complete benchmark data, processed tables and plots are included under:

```text
benchmark/results/
```

## Performance Analysis

The benchmark results are analyzed using runtime, speedup, parallel efficiency, and MPI phase timing.

### Runtime vs. Number of Processes

Runtime is measured for each dataset size across the tested MPI process counts. The sequential runtime is also shown as a reference.

#### N = 10,000
![Runtime vs Processes — N=10,000](benchmark/results/plots/runtime_vs_processes_N10000.png)

#### N = 100,000
![Runtime vs Processes — N=100,000](benchmark/results/plots/runtime_vs_processes_N100000.png)

#### N = 1,000,000
![Runtime vs Processes — N=1,000,000](benchmark/results/plots/runtime_vs_processes_N1000000.png)

### Speedup

Speedup is calculated as `T_seq / T_MPI(P)` and compared with ideal linear speedup.

#### N = 10,000
![Speedup vs Processes — N=10,000](benchmark/results/plots/speedup_vs_processes_N10000.png)

#### N = 100,000
![Speedup vs Processes — N=100,000](benchmark/results/plots/speedup_vs_processes_N100000.png)

#### N = 1,000,000
![Speedup vs Processes — N=1,000,000](benchmark/results/plots/speedup_vs_processes_N1000000.png)

### Parallel Efficiency

Parallel efficiency is calculated as `Speedup / P`.

#### N = 10,000
![Efficiency vs Processes — N=10,000](benchmark/results/plots/efficiency_vs_processes_N10000.png)

#### N = 100,000
![Efficiency vs Processes — N=100,000](benchmark/results/plots/efficiency_vs_processes_N100000.png)

#### N = 1,000,000
![Efficiency vs Processes — N=1,000,000](benchmark/results/plots/efficiency_vs_processes_N1000000.png)

### Scaling with Input Size

The following plots show how runtime and speedup change as the number of measurements increases.

![Runtime vs Input Size](benchmark/results/plots/runtime_vs_input_size.png)

![Speedup vs Input Size](benchmark/results/plots/speedup_vs_input_size.png)

### MPI Phase Analysis

The MPI implementation records distribution, computation, aggregation, final-processing, and communication times.

![MPI Phase Times — N=10,000](benchmark/results/plots/mpi_phase_times_N10000.png)

![MPI Phase Times — N=100,000](benchmark/results/plots/mpi_phase_times_N100000.png)

![MPI Phase Times — N=1,000,000](benchmark/results/plots/mpi_phase_times_N1000000.png)

The fraction of total runtime spent in each phase is shown below.

![MPI Phase Runtime Percentage — N=10,000](benchmark/results/plots/mpi_phase_percentage_N10000.png)

![MPI Phase Runtime Percentage — N=100,000](benchmark/results/plots/mpi_phase_percentage_N100000.png)

![MPI Phase Runtime Percentage — N=1,000,000](benchmark/results/plots/mpi_phase_percentage_N1000000.png)

