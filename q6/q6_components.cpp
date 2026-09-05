#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <mpi.h>
#include <algorithm>

using namespace std;

// Sequential Label Propagation (for correctness verification baseline)
void sequentialCC(int V, const vector<vector<int>>& adj, vector<int>& comp) {
    for (int i = 0; i < V; i++) comp[i] = i;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int u = 0; u < V; u++) {
            for (int v : adj[u]) {
                if (comp[u] < comp[v]) {
                    comp[v] = comp[u];
                    changed = true;
                }
                if (comp[v] < comp[u]) {
                    comp[u] = comp[v];
                    changed = true;
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, P;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    int V = 0;
    vector<vector<int>> local_adj;
    vector<int> local_vertices;

    double start_time = MPI_Wtime();

    // 1. Data Distribution Phase
    if (rank == 0) {
        ifstream in("input.txt");
        if (!in.is_open()) {
            cout << "Error opening input.txt\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        in >> V;
        string line;
        getline(in, line); // consume newline

        MPI_Bcast(&V, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // Read edges line by line and distribute vertices to their owner process
        for (int i = 0; i < V; i++) {
            getline(in, line);
            stringstream ss(line);
            int k;
            ss >> k;
            vector<int> neighbors(k);
            for (int j = 0; j < k; j++) ss >> neighbors[j];

            int owner = (i * P) / V; // Even distribution
            if (owner == 0) {
                local_vertices.push_back(i);
                local_adj.push_back(neighbors);
            } else {
                MPI_Send(&i, 1, MPI_INT, owner, 0, MPI_COMM_WORLD);
                MPI_Send(&k, 1, MPI_INT, owner, 0, MPI_COMM_WORLD);
                if (k > 0) MPI_Send(neighbors.data(), k, MPI_INT, owner, 0, MPI_COMM_WORLD);
            }
        }
        in.close();
        
        // Send a termination signal (-1) so worker processes know when to stop waiting for data
        for (int p = 1; p < P; p++) {
            int term = -1;
            MPI_Send(&term, 1, MPI_INT, p, 0, MPI_COMM_WORLD);
        }
    } else {
        // Workers receive V
        MPI_Bcast(&V, 1, MPI_INT, 0, MPI_COMM_WORLD);
        while (true) {
            int v_id, k;
            MPI_Recv(&v_id, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (v_id == -1) break; // Reached end of data for this process
            MPI_Recv(&k, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            vector<int> neighbors(k);
            if (k > 0) MPI_Recv(neighbors.data(), k, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            local_vertices.push_back(v_id);
            local_adj.push_back(neighbors);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double comp_start = MPI_Wtime();

    // 2. Initialization Phase
    vector<int> comp(V);
    for (int i = 0; i < V; i++) comp[i] = i; // Every vertex thinks it is its own component ID

    // 3. Parallel Label Propagation Loop
    int iterations = 0;
    bool changed = true;
    while (changed) {
        iterations++;
        vector<int> old_comp = comp;

        // Local Updates
        for (size_t i = 0; i < local_vertices.size(); i++) {
            int u = local_vertices[i];
            for (int v : local_adj[i]) {
                // Find the smallest ID between the two connected vertices
                int min_c = min(comp[u], comp[v]);
                comp[u] = min(comp[u], min_c);
                comp[v] = min(comp[v], min_c);
            }
        }

        // Global Synchronization (Message Passing)
        // MPI_Allreduce finds the absolute minimum known component ID for every vertex across all processes
        MPI_Allreduce(MPI_IN_PLACE, comp.data(), V, MPI_INT, MPI_MIN, MPI_COMM_WORLD);

        // Check if any process made an update this round
        changed = false;
        for (int i = 0; i < V; i++) {
            if (comp[i] != old_comp[i]) {
                changed = true;
                break;
            }
        }
    }

    double comp_time = MPI_Wtime() - comp_start;

    // 4. Output, Verification & Analysis (Rank 0 only)
    if (rank == 0) {
        // Re-read file to build full adj list for sequential baseline testing
        ifstream in("input.txt");
        int dummy_V; in >> dummy_V;
        vector<vector<int>> full_adj(V);
        for (int i = 0; i < V; i++) {
            int k; in >> k;
            full_adj[i].resize(k);
            for (int j = 0; j < k; j++) in >> full_adj[i][j];
        }
        in.close();

        // Calculate Sequential Time (T1 Baseline)
        double seq_start = MPI_Wtime();
        vector<int> seq_comp(V);
        sequentialCC(V, full_adj, seq_comp);
        double seq_time = MPI_Wtime() - seq_start;

        // Verify Correctness
        bool correct = true;
        for (int i = 0; i < V; i++) {
            if (comp[i] != seq_comp[i]) {
                correct = false;
                break;
            }
        }

        // Write strict homework output format to output.txt
        ofstream out("output.txt");
        for (int i = 0; i < V; i++) {
            out << i << " " << comp[i] << "\n";
        }
        out.close();

        // Write benchmark metrics to analysis.txt
        ofstream analysis("analysis.txt", ios_base::app);
        analysis << "P=" << P << " | V=" << V << " | Iterations=" << iterations 
                 << " | Correct=" << (correct ? "YES" : "NO") 
                 << " | MPI Time=" << comp_time << "s | Seq Time=" << seq_time 
                 << "s | Speedup=" << seq_time / comp_time << "\n";
        analysis.close();

        cout << "P=" << P << " | V=" << V << " | Iterations: " << iterations 
             << " | Status: " << (correct ? "SUCCESS" : "FAILED") 
             << " | Time: " << comp_time << "s\n";
    }

    MPI_Finalize();
    return 0;
}
