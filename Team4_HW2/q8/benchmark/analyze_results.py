#!/usr/bin/env python3

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

from pathlib import Path


# ============================================================
# Paths
# ============================================================

SCRIPT_DIR = Path(__file__).resolve().parent

RESULTS_DIR = SCRIPT_DIR / "results"
RESULT_FILE = RESULTS_DIR / "benchmark_summary.csv"

PLOTS_DIR = RESULTS_DIR / "plots"
PROCESSED_DIR = RESULTS_DIR / "processed"

PLOTS_DIR.mkdir(parents=True, exist_ok=True)
PROCESSED_DIR.mkdir(parents=True, exist_ok=True)


# ============================================================
# Configuration
# ============================================================

# Sequential implementation is used as the reference.
SEQ_TYPE = "seq"

# MPI implementation.
MPI_TYPE = "mpi"


# ============================================================
# Read raw benchmark results
# ============================================================

if not RESULT_FILE.exists():
    raise FileNotFoundError(
        f"Could not find benchmark results:\n{RESULT_FILE}"
    )


df = pd.read_csv(RESULT_FILE)


required_columns = {
    "N",
    "type",
    "processes",
    "run",
    "total_time",
    "distribution_time",
    "computation_time",
    "aggregation_time",
    "final_processing_time",
    "communication_time",
}


missing = required_columns - set(df.columns)

if missing:
    raise ValueError(
        "benchmark_summary.csv is missing columns:\n"
        + ", ".join(sorted(missing))
    )


# ============================================================
# Basic cleanup
# ============================================================

df["N"] = pd.to_numeric(df["N"])
df["processes"] = pd.to_numeric(df["processes"])
df["run"] = pd.to_numeric(df["run"])

numeric_columns = [
    "total_time",
    "distribution_time",
    "computation_time",
    "aggregation_time",
    "final_processing_time",
    "communication_time",
]

for column in numeric_columns:
    df[column] = pd.to_numeric(
        df[column],
        errors="coerce"
    )


# ============================================================
# Print basic information
# ============================================================

print()
print("=" * 80)
print("BENCHMARK DATA")
print("=" * 80)
print()

print(f"Input file : {RESULT_FILE}")
print(f"Total runs : {len(df)}")
print()

print("Datasets:")
for N in sorted(df["N"].unique()):
    print(f"  N = {N:,}")

print()

print("MPI process counts:")
mpi_processes = sorted(
    df.loc[df["type"] == MPI_TYPE, "processes"]
    .dropna()
    .unique()
)

for p in mpi_processes:
    print(f"  P = {int(p)}")

print()


# ============================================================
# Median runtime
#
# Multiple runs are used because individual runtimes can vary.
# Median is used as the representative runtime.
# ============================================================

group_columns = [
    "N",
    "type",
    "processes",
]

median_df = (
    df
    .groupby(group_columns, as_index=False)
    .agg(
        total_time=("total_time", "median"),
        distribution_time=("distribution_time", "median"),
        computation_time=("computation_time", "median"),
        aggregation_time=("aggregation_time", "median"),
        final_processing_time=("final_processing_time", "median"),
        communication_time=("communication_time", "median"),
        runs=("run", "count"),
    )
)


median_df = median_df.sort_values(
    ["N", "type", "processes"]
)


median_file = PROCESSED_DIR / "median_results.csv"

median_df.to_csv(
    median_file,
    index=False
)


# ============================================================
# Sequential reference
#
# For every N, obtain the median sequential runtime.
# ============================================================

seq_df = median_df[
    median_df["type"] == SEQ_TYPE
].copy()


seq_reference = (
    seq_df[
        ["N", "total_time"]
    ]
    .rename(
        columns={
            "total_time": "seq_time"
        }
    )
)


# ============================================================
# MPI results
# ============================================================

mpi_df = median_df[
    median_df["type"] == MPI_TYPE
].copy()


# ============================================================
# Speedup and efficiency
#
# Speedup:
#
#     S(P) = T_seq / T_parallel(P)
#
# Efficiency:
#
#     E(P) = S(P) / P
# ============================================================

analysis_df = mpi_df.merge(
    seq_reference,
    on="N",
    how="left"
)


analysis_df["speedup"] = (
    analysis_df["seq_time"] /
    analysis_df["total_time"]
)


analysis_df["efficiency"] = (
    analysis_df["speedup"] /
    analysis_df["processes"]
)


analysis_df["efficiency_percent"] = (
    analysis_df["efficiency"] * 100.0
)


analysis_df = analysis_df.sort_values(
    ["N", "processes"]
)


analysis_file = (
    PROCESSED_DIR /
    "speedup_efficiency.csv"
)


analysis_df.to_csv(
    analysis_file,
    index=False
)


# ============================================================
# Runtime table
# ============================================================

print("=" * 80)
print("RUNTIME TABLE")
print("=" * 80)
print()

all_processes = sorted(
    mpi_df["processes"].unique()
)

header = (
    f"{'N':>12}"
    f"{'Sequential':>15}"
)

for p in all_processes:
    header += f"{'MPI P=' + str(int(p)):>15}"

print(header)
print("-" * len(header))


for N in sorted(df["N"].unique()):

    seq_match = seq_df[
        seq_df["N"] == N
    ]

    if seq_match.empty:
        continue

    seq_time = seq_match.iloc[0]["total_time"]

    line = (
        f"{int(N):>12,}"
        f"{seq_time:>15.6f}"
    )

    for p in all_processes:

        match = mpi_df[
            (mpi_df["N"] == N) &
            (mpi_df["processes"] == p)
        ]

        if match.empty:
            line += f"{'N/A':>15}"
        else:
            time = match.iloc[0]["total_time"]
            line += f"{time:>15.6f}"

    print(line)


# ============================================================
# Speedup / efficiency table
# ============================================================

print()
print("=" * 80)
print("SPEEDUP / PARALLEL EFFICIENCY")
print("=" * 80)
print()

print(
    f"{'N':>12}"
    f"{'P':>8}"
    f"{'Seq Time':>15}"
    f"{'MPI Time':>15}"
    f"{'Speedup':>12}"
    f"{'Efficiency':>15}"
)

print("-" * 82)


for _, row in analysis_df.iterrows():

    print(
        f"{int(row['N']):>12,}"
        f"{int(row['processes']):>8}"
        f"{row['seq_time']:>15.6f}"
        f"{row['total_time']:>15.6f}"
        f"{row['speedup']:>12.3f}"
        f"{row['efficiency_percent']:>14.2f}%"
    )


# ============================================================
# Plot 1:
# Runtime vs number of processes
#
# One plot for each dataset size.
# ============================================================

for N in sorted(mpi_df["N"].unique()):

    subset = mpi_df[
        mpi_df["N"] == N
    ].sort_values("processes")

    if subset.empty:
        continue

    plt.figure()

    plt.plot(
        subset["processes"],
        subset["total_time"],
        marker="o"
    )

    # Add sequential reference if available.
    seq_match = seq_df[
        seq_df["N"] == N
    ]

    if not seq_match.empty:

        seq_time = seq_match.iloc[0]["total_time"]

        plt.axhline(
            seq_time,
            linestyle="--",
            label="Sequential"
        )

    plt.xlabel("Number of Processes (P)")
    plt.ylabel("Runtime (seconds)")
    plt.title(
        f"MPI Runtime vs Processes (N={int(N):,})"
    )

    plt.xticks(
        subset["processes"].astype(int)
    )

    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    filename = (
        PLOTS_DIR /
        f"runtime_vs_processes_N{int(N)}.png"
    )

    plt.savefig(
        filename,
        dpi=300
    )

    plt.close()


# ============================================================
# Plot 2:
# Speedup vs number of processes
# ============================================================

for N in sorted(analysis_df["N"].unique()):

    subset = analysis_df[
        analysis_df["N"] == N
    ].sort_values("processes")

    if subset.empty:
        continue

    plt.figure()

    plt.plot(
        subset["processes"],
        subset["speedup"],
        marker="o",
        label="Measured Speedup"
    )

    # Ideal linear speedup.
    max_p = subset["processes"].max()

    ideal_x = np.array(
        sorted(subset["processes"].unique())
    )

    ideal_y = ideal_x

    plt.plot(
        ideal_x,
        ideal_y,
        linestyle="--",
        label="Ideal Linear Speedup"
    )

    plt.xlabel("Number of Processes (P)")
    plt.ylabel("Speedup")
    plt.title(
        f"Speedup vs Processes (N={int(N):,})"
    )

    plt.xticks(
        subset["processes"].astype(int)
    )

    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    filename = (
        PLOTS_DIR /
        f"speedup_vs_processes_N{int(N)}.png"
    )

    plt.savefig(
        filename,
        dpi=300
    )

    plt.close()


# ============================================================
# Plot 3:
# Parallel efficiency vs number of processes
# ============================================================

for N in sorted(analysis_df["N"].unique()):

    subset = analysis_df[
        analysis_df["N"] == N
    ].sort_values("processes")

    if subset.empty:
        continue

    plt.figure()

    plt.plot(
        subset["processes"],
        subset["efficiency_percent"],
        marker="o"
    )

    plt.axhline(
        100,
        linestyle="--",
        label="Ideal Efficiency"
    )

    plt.xlabel("Number of Processes (P)")
    plt.ylabel("Parallel Efficiency (%)")
    plt.title(
        f"Parallel Efficiency vs Processes (N={int(N):,})"
    )

    plt.xticks(
        subset["processes"].astype(int)
    )

    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    filename = (
        PLOTS_DIR /
        f"efficiency_vs_processes_N{int(N)}.png"
    )

    plt.savefig(
        filename,
        dpi=300
    )

    plt.close()


# ============================================================
# Plot 4:
# Runtime vs input size
#
# Separate lines for sequential and each MPI process count.
# ============================================================

plt.figure()

seq_plot = (
    seq_df
    .sort_values("N")
)

if not seq_plot.empty:

    plt.plot(
        seq_plot["N"],
        seq_plot["total_time"],
        marker="o",
        label="Sequential"
    )


for p in all_processes:

    subset = (
        mpi_df[
            mpi_df["processes"] == p
        ]
        .sort_values("N")
    )

    if subset.empty:
        continue

    plt.plot(
        subset["N"],
        subset["total_time"],
        marker="o",
        label=f"MPI P={int(p)}"
    )


plt.xlabel("Number of Measurements (N)")
plt.ylabel("Runtime (seconds)")
plt.title("Runtime vs Input Size")

plt.grid(True, alpha=0.3)
plt.legend()
plt.tight_layout()

plt.savefig(
    PLOTS_DIR / "runtime_vs_input_size.png",
    dpi=300
)

plt.close()


# ============================================================
# Plot 5:
# Speedup vs input size
#
# One line for each process count.
# ============================================================

plt.figure()

for p in all_processes:

    subset = (
        analysis_df[
            analysis_df["processes"] == p
        ]
        .sort_values("N")
    )

    if subset.empty:
        continue

    plt.plot(
        subset["N"],
        subset["speedup"],
        marker="o",
        label=f"P={int(p)}"
    )


plt.xlabel("Number of Measurements (N)")
plt.ylabel("Speedup")
plt.title("Speedup vs Input Size")

plt.grid(True, alpha=0.3)
plt.legend()
plt.tight_layout()

plt.savefig(
    PLOTS_DIR / "speedup_vs_input_size.png",
    dpi=300
)

plt.close()


# ============================================================
# MPI timing phase analysis
#
# These measurements come from the instrumentation:
#
#   distribution
#   computation
#   aggregation
#   final processing
#   communication
# ============================================================

phase_columns = [
    "distribution_time",
    "computation_time",
    "aggregation_time",
    "final_processing_time",
    "communication_time",
]


# ------------------------------------------------------------
# Phase table
# ------------------------------------------------------------

phase_df = mpi_df[
    [
        "N",
        "processes",
        "total_time",
    ] + phase_columns
].copy()


phase_file = (
    PROCESSED_DIR /
    "mpi_phase_times.csv"
)


phase_df.to_csv(
    phase_file,
    index=False
)


# ============================================================
# Plot 6:
# MPI phase breakdown
#
# A separate plot for each N.
# ============================================================

for N in sorted(mpi_df["N"].unique()):

    subset = mpi_df[
        mpi_df["N"] == N
    ].sort_values("processes")

    if subset.empty:
        continue

    plt.figure()

    for phase in phase_columns:

        plt.plot(
            subset["processes"],
            subset[phase],
            marker="o",
            label=phase.replace(
                "_time",
                ""
            ).replace(
                "_",
                " "
            ).title()
        )

    plt.xlabel("Number of Processes (P)")
    plt.ylabel("Time (seconds)")
    plt.title(
        f"MPI Phase Times vs Processes (N={int(N):,})"
    )

    plt.xticks(
        subset["processes"].astype(int)
    )

    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    filename = (
        PLOTS_DIR /
        f"mpi_phase_times_N{int(N)}.png"
    )

    plt.savefig(
        filename,
        dpi=300
    )

    plt.close()


# ============================================================
# Plot 7:
# Fraction of total runtime spent in each MPI phase
#
# Useful for understanding where MPI execution time goes.
# ============================================================

for N in sorted(mpi_df["N"].unique()):

    subset = mpi_df[
        mpi_df["N"] == N
    ].sort_values("processes")

    if subset.empty:
        continue

    plt.figure()

    for phase in phase_columns:

        percentage = (
            subset[phase] /
            subset["total_time"] *
            100.0
        )

        plt.plot(
            subset["processes"],
            percentage,
            marker="o",
            label=phase.replace(
                "_time",
                ""
            ).replace(
                "_",
                " "
            ).title()
        )

    plt.xlabel("Number of Processes (P)")
    plt.ylabel("Percentage of Total Runtime (%)")
    plt.title(
        f"MPI Phase Runtime Percentage (N={int(N):,})"
    )

    plt.xticks(
        subset["processes"].astype(int)
    )

    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    filename = (
        PLOTS_DIR /
        f"mpi_phase_percentage_N{int(N)}.png"
    )

    plt.savefig(
        filename,
        dpi=300
    )

    plt.close()


# ============================================================
# Best process count for each dataset
# ============================================================

best_rows = []

for N in sorted(analysis_df["N"].unique()):

    subset = analysis_df[
        analysis_df["N"] == N
    ]

    if subset.empty:
        continue

    best = subset.loc[
        subset["total_time"].idxmin()
    ]

    best_rows.append({
        "N": int(N),
        "best_processes": int(
            best["processes"]
        ),
        "best_runtime": best["total_time"],
        "speedup": best["speedup"],
        "efficiency": best["efficiency"],
        "efficiency_percent":
            best["efficiency_percent"],
    })


best_df = pd.DataFrame(best_rows)

best_file = (
    PROCESSED_DIR /
    "best_process_count.csv"
)

best_df.to_csv(
    best_file,
    index=False
)


# ============================================================
# Final summary
# ============================================================

print()
print("=" * 80)
print("BEST MPI CONFIGURATION FOR EACH INPUT SIZE")
print("=" * 80)
print()

print(
    f"{'N':>12}"
    f"{'Best P':>10}"
    f"{'Runtime':>15}"
    f"{'Speedup':>12}"
    f"{'Efficiency':>15}"
)

print("-" * 66)

for _, row in best_df.iterrows():

    print(
        f"{int(row['N']):>12,}"
        f"{int(row['best_processes']):>10}"
        f"{row['best_runtime']:>15.6f}"
        f"{row['speedup']:>12.3f}"
        f"{row['efficiency_percent']:>14.2f}%"
    )


print()
print("=" * 80)
print("OUTPUT FILES")
print("=" * 80)
print()

print("Processed CSV files:")
print(f"  {median_file}")
print(f"  {analysis_file}")
print(f"  {phase_file}")
print(f"  {best_file}")

print()
print("Plots:")
print(f"  {PLOTS_DIR}")

print()
print("Analysis complete.")
print()