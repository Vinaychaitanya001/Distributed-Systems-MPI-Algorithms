#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <cstdlib>

using namespace std;

int main(int argc, char** argv) {
    // Default to 10,000 vertices with max degree of 20
    int V = 10000;
    int max_degree = 20;
    
    if (argc > 1) V = atoi(argv[1]);
    if (argc > 2) max_degree = atoi(argv[2]);

    srand(12345); // Fixed seed for reproducibility

    // Generate random undirected edges
    vector<set<int>> adj(V);
    for (int i = 0; i < V; i++) {
        int deg = rand() % max_degree + 1;
        for (int d = 0; d < deg; d++) {
            int neighbor = rand() % V;
            if (neighbor != i) {
                adj[i].insert(neighbor);
                adj[neighbor].insert(i); // Ensure it is an undirected graph
            }
        }
    }

    // Write to input.txt in the exact format required by the homework
    ofstream out("input.txt");
    out << V << "\n";
    for (int i = 0; i < V; i++) {
        out << adj[i].size();
        for (int neighbor : adj[i]) {
            out << " " << neighbor;
        }
        out << "\n";
    }
    out.close();

    cout << "Generated input.txt with " << V << " vertices successfully.\n";
    return 0;
}
