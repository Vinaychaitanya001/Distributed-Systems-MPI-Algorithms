#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <mpi.h>
#include <cstdlib>
#include <ctime>

using namespace std;

// --- Custom Bitonic Sort implementation (No std::sort) ---

// Compare and swap elements based on direction
// direction = 1 -> ascending, direction = 0 -> descending
void compAndSwap(vector<int>& arr, int i, int j, int direction) {
    if ((direction == 1 && arr[i] > arr[j]) || (direction == 0 && arr[i] < arr[j])) {
        swap(arr[i], arr[j]);
    }
}

// Iteratively merge a bitonic sequence into sorted order
// Swap at decreasing distances: cnt/2, cnt/4, ..., 1
void bitonicMerge(vector<int>& arr, int low, int cnt, int direction) {
    for (int k = cnt / 2; k >= 1; k /= 2) {
        for (int j = low; j < low + cnt - k; j++) {
            // Only compare elements within the same block of size 2*k
            if (((j - low) % (2 * k)) < k) {
                compAndSwap(arr, j, j + k, direction);
            }
        }
    }
}

// Recursively build bitonic sequences and sort them
void bitonicSortSeq(vector<int>& arr, int low, int cnt, int direction) {
    if (cnt > 1) {
        int k = cnt / 2;
        // Sort first half ascending
        bitonicSortSeq(arr, low, k, 1);
        // Sort second half descending
        bitonicSortSeq(arr, low + k, k, 0);
        // Merge entire sequence in given direction
        bitonicMerge(arr, low, cnt, direction);
    }
}

// ---------------------------------------------------------

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, P;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    long long N = 0;
    vector<int> original_arr;
    
    if (rank == 0) {
        ifstream in("input.txt");
        if (!in.is_open()) {
            cout << "Error opening input.txt\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        in >> N;
        original_arr.resize(N);
        for (long long i = 0; i < N; i++) {
            in >> original_arr[i];
        }
        in.close();
    }
    
    // Broadcast N to all processes
    MPI_Bcast(&N, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    
    if (N % P != 0) {
        if (rank == 0) cout << "Error: N (" << N << ") must be divisible by P (" << P << ")." << endl;
        MPI_Finalize();
        return 1;
    }

    long long L = N / P;
    vector<int> local_arr(L);

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    // 1. Distribute Data
    double comm_start = MPI_Wtime();
    MPI_Scatter(rank == 0 ? original_arr.data() : nullptr, L, MPI_INT,
                local_arr.data(), L, MPI_INT,
                0, MPI_COMM_WORLD);
    double comm_time = MPI_Wtime() - comm_start;
    double comp_time = 0.0;

    // 2. Initial Local Sort (Using Custom Bitonic Sort)
    double comp_start = MPI_Wtime();
    bool sort_dir_asc = ((rank & 1) == 0);
    bitonicSortSeq(local_arr, 0, L, sort_dir_asc ? 1 : 0);
    comp_time += (MPI_Wtime() - comp_start);

    // Calculate d = log2(P) safely for integers
    int d = 0;
    int temp = P;
    while (temp > 1) {
        temp >>= 1;
        d++;
    }

    // 3. Bitonic Merge Network
    for (int i = 0; i < d; i++) {
        for (int j = i; j >= 0; j--) {
            int partner = rank ^ (1 << j);
            bool block_asc = ((rank & (1 << (i + 1))) == 0);
            bool keep_small = ((rank < partner) == block_asc);

            vector<int> partner_arr(L);
            
            comm_start = MPI_Wtime();
            MPI_Sendrecv(local_arr.data(), L, MPI_INT, partner, 0,
                         partner_arr.data(), L, MPI_INT, partner, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            comm_time += (MPI_Wtime() - comm_start);

            comp_start = MPI_Wtime();
            
            // To properly do a position-wise bitonic split, one array must be ASC and the other DESC.
            // If they are both sorted in the same direction, we reverse the partner's array.
            bool my_asc = (L == 1) ? true : (local_arr[0] <= local_arr[L - 1]);
            bool partner_asc = (L == 1) ? true : (partner_arr[0] <= partner_arr[L - 1]);
            
            if (my_asc == partner_asc) {
                reverse(partner_arr.begin(), partner_arr.end());
            }

            // Compare position-wise and keep min/max
            for (long long k = 0; k < L; k++) {
                int my_val = local_arr[k];
                int their_val = partner_arr[k];
                if (keep_small) {
                    local_arr[k] = min(my_val, their_val);
                } else {
                    local_arr[k] = max(my_val, their_val);
                }
            }

            // Re-sort locally
            bitonicSortSeq(local_arr, 0, L, block_asc ? 1 : 0);
            
            comp_time += (MPI_Wtime() - comp_start);
        }
    }

    // 4. Gather Result
    vector<int> final_arr;
    if (rank == 0) final_arr.resize(N);

    comm_start = MPI_Wtime();
    MPI_Gather(local_arr.data(), L, MPI_INT,
               rank == 0 ? final_arr.data() : nullptr, L, MPI_INT,
               0, MPI_COMM_WORLD);
    comm_time += (MPI_Wtime() - comm_start);

    double total_time = MPI_Wtime() - start_time;

    // 5. Verification & Output (Only Rank 0)
    if (rank == 0) {
        ofstream out("output.txt");
        for (long long i = 0; i < N; i++) {
            out << final_arr[i] << (i == N - 1 ? "" : " ");
        }
        out << "\n";
        out.close();

        bool correct = true;
        
        // Measure sequential time using Custom Bitonic Sort for benchmark baseline
        double seq_start = MPI_Wtime();
        vector<int> seq_arr = original_arr; 
        bitonicSortSeq(seq_arr, 0, N, 1);
        double seq_time = MPI_Wtime() - seq_start;

        for (long long i = 0; i < N; i++) {
            if (final_arr[i] != seq_arr[i]) {
                correct = false;
                break;
            }
        }

        cout << "P=" << P << " | N=" << N;
        if (correct) cout << " | Status: SUCCESS";
        else cout << " | Status: FAILED";
        
        cout << " | Total: " << total_time << "s";
        cout << " | Comp: " << comp_time << "s";
        cout << " | Comm: " << comm_time << "s";
        cout << " | Seq (T1): " << seq_time << "s";
        cout << " | Speedup: " << seq_time / total_time;
        cout << " | Efficiency: " << (seq_time / total_time) / P << endl;
    }

    MPI_Finalize();
    return 0;
}
