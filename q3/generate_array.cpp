#include <iostream>
#include <fstream>
#include <cstdlib>

using namespace std;

int main(int argc, char** argv) {
    long long N = 16;
    if (argc > 1) {
        N = atoll(argv[1]);
    }
    
    srand(12345);
    ofstream out("input.txt");
    if (!out.is_open()) {
        cout << "Error opening input.txt for writing.\n";
        return 1;
    }
    
    out << N << "\n";
    for (long long i = 0; i < N; i++) {
        out << (rand() % 1000000) << (i == N - 1 ? "" : " ");
    }
    out << "\n";
    out.close();
    
    cout << "Generated input.txt with " << N << " elements successfully.\n";
    return 0;
}
