#!/usr/bin/env python3

from pathlib import Path
import sys


# ============================================================
# Paths
# ============================================================

SCRIPT_DIR = Path(__file__).resolve().parent
RESULTS_DIR = SCRIPT_DIR / "results"


# ============================================================
# Configuration
# ============================================================

DATASET_SIZES = [
    10000,
    100000,
    1000000,
]

MPI_PROCESSES = [
    1,
    2,
    4,
    8,
]

RUNS = 5


# ============================================================
# Normalize output
# ============================================================

def normalize_output(path):
    """
    Read program output and normalize harmless formatting
    differences such as trailing whitespace and blank lines.
    """

    with open(path, "r") as f:
        lines = f.readlines()

    # Remove trailing whitespace from every line.
    lines = [
        line.rstrip()
        for line in lines
    ]

    # Remove empty lines at beginning/end.
    while lines and not lines[0]:
        lines.pop(0)

    while lines and not lines[-1]:
        lines.pop()

    return lines


# ============================================================
# Compare two output files
# ============================================================

def compare_outputs(seq_file, mpi_file):

    seq_output = normalize_output(seq_file)
    mpi_output = normalize_output(mpi_file)

    if seq_output == mpi_output:
        return True, None

    # Find first differing line.
    max_lines = max(
        len(seq_output),
        len(mpi_output)
    )

    for i in range(max_lines):

        seq_line = (
            seq_output[i]
            if i < len(seq_output)
            else "<missing>"
        )

        mpi_line = (
            mpi_output[i]
            if i < len(mpi_output)
            else "<missing>"
        )

        if seq_line != mpi_line:
            return False, (
                i + 1,
                seq_line,
                mpi_line
            )

    return False, (
        "unknown",
        "",
        ""
    )


# ============================================================
# Main
# ============================================================

print()
print("=" * 70)
print("Q8 BENCHMARK CORRECTNESS VERIFICATION")
print("=" * 70)
print()

print(f"Results directory: {RESULTS_DIR}")
print()

total_tests = 0
passed_tests = 0
failed_tests = 0
missing_tests = 0


for N in DATASET_SIZES:

    print()
    print("-" * 70)
    print(f"DATASET N={N:,}")
    print("-" * 70)

    # --------------------------------------------------------
    # Use sequential run 1 as the reference.
    #
    # All sequential runs should produce identical logical
    # output, since they use the same deterministic dataset.
    # --------------------------------------------------------

    seq_reference = (
        RESULTS_DIR /
        f"stdout_seq_{N}_run1.txt"
    )

    if not seq_reference.exists():

        print(
            f"ERROR: Sequential reference not found:\n"
            f"       {seq_reference}"
        )

        missing_tests += 1
        continue

    # --------------------------------------------------------
    # First verify that all sequential runs agree.
    # --------------------------------------------------------

    for run in range(1, RUNS + 1):

        seq_file = (
            RESULTS_DIR /
            f"stdout_seq_{N}_run{run}.txt"
        )

        if not seq_file.exists():

            print(
                f"  MISSING Sequential run {run}"
            )

            missing_tests += 1
            continue

        total_tests += 1

        ok, difference = compare_outputs(
            seq_reference,
            seq_file
        )

        if ok:

            passed_tests += 1

        else:

            failed_tests += 1

            line_no, seq_line, other_line = difference

            print()
            print(
                f"  FAIL: Sequential run {run}"
            )
            print(
                f"        Difference at line {line_no}"
            )
            print(
                f"        Reference: {seq_line}"
            )
            print(
                f"        Run {run}:  {other_line}"
            )

    # --------------------------------------------------------
    # Compare every MPI run against the sequential reference.
    # --------------------------------------------------------

    for P in MPI_PROCESSES:

        for run in range(1, RUNS + 1):

            mpi_file = (
                RESULTS_DIR /
                f"stdout_mpi_{N}_p{P}_run{run}.txt"
            )

            if not mpi_file.exists():

                print(
                    f"  MISSING MPI P={P} run={run}"
                )

                missing_tests += 1
                continue

            total_tests += 1

            ok, difference = compare_outputs(
                seq_reference,
                mpi_file
            )

            if ok:

                passed_tests += 1

                print(
                    f"  PASS: MPI P={P:<2} run={run}"
                )

            else:

                failed_tests += 1

                line_no, seq_line, mpi_line = difference

                print()
                print(
                    f"  FAIL: MPI P={P:<2} run={run}"
                )
                print(
                    f"        Difference at line {line_no}"
                )
                print(
                    f"        Sequential: {seq_line}"
                )
                print(
                    f"        MPI:        {mpi_line}"
                )


# ============================================================
# Summary
# ============================================================

print()
print("=" * 70)
print("VERIFICATION SUMMARY")
print("=" * 70)
print()

print(f"Tests checked : {total_tests}")
print(f"Passed        : {passed_tests}")
print(f"Failed        : {failed_tests}")
print(f"Missing       : {missing_tests}")

print()

if failed_tests == 0 and missing_tests == 0:

    print("ALL BENCHMARK OUTPUTS MATCH THE SEQUENTIAL REFERENCE.")
    print()
    print("MPI correctness verification PASSED.")

    sys.exit(0)

else:

    print("BENCHMARK CORRECTNESS VERIFICATION FAILED.")

    if failed_tests > 0:
        print(f"  {failed_tests} output comparison(s) failed.")

    if missing_tests > 0:
        print(f"  {missing_tests} output file(s) were missing.")

    print()

    sys.exit(1)