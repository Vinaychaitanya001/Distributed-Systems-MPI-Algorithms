import matplotlib.pyplot as plt
import re
from collections import defaultdict

speedups = defaultdict(dict)
efficiencies = defaultdict(dict)

print("Reading analysis.txt...")

try:
    with open('analysis.txt', 'r') as f:
        for line in f:
            if "P=" in line and "N=" in line and "Speedup:" in line:
                try:
                    # Example: P=1 | N=16 | Status: SUCCESS | Total: ... | Speedup: 0.127294 | Efficiency: 0.127294
                    p_match = re.search(r'P=(\d+)', line)
                    n_match = re.search(r'N=(\d+)', line)
                    speedup_match = re.search(r'Speedup:\s*([0-9\.e\-]+)', line)
                    efficiency_match = re.search(r'Efficiency:\s*([0-9\.e\-]+)', line)
                    
                    if p_match and n_match and speedup_match and efficiency_match:
                        P = int(p_match.group(1))
                        N = int(n_match.group(1))
                        speedup = float(speedup_match.group(1))
                        efficiency = float(efficiency_match.group(1))
                        
                        speedups[N][P] = speedup
                        efficiencies[N][P] = efficiency
                except Exception as e:
                    pass
except FileNotFoundError:
    print("Error: analysis.txt not found. Please pull it from the cluster first.")
    exit(1)

if not speedups:
    print("Error: No valid data found in analysis.txt")
    exit(1)

# 1. Plot Speedup
plt.figure(figsize=(10, 6))
for N, p_data in sorted(speedups.items()):
    P_vals = sorted(p_data.keys())
    S_vals = [p_data[p] for p in P_vals]
    plt.plot(P_vals, S_vals, marker='o', linewidth=2, label=f'N={N} elements')

max_p = max([max(p_data.keys()) for p_data in speedups.values()])
plt.plot([1, max_p], [1, max_p], 'k--', label='Ideal Speedup')

plt.title('Speedup vs Number of Processes (Q3: Bitonic Sort)')
plt.xlabel('Number of Processes (P)')
plt.ylabel('Speedup (T1 / Tp)')
plt.xticks(sorted(list(speedups.values())[0].keys()))
plt.grid(True, linestyle='--', alpha=0.7)
plt.legend()
plt.savefig('speedup_plot.png', dpi=300)
print("SUCCESS: Saved speedup_plot.png")

# 2. Plot Efficiency
plt.figure(figsize=(10, 6))
for N, p_data in sorted(efficiencies.items()):
    P_vals = sorted(p_data.keys())
    E_vals = [p_data[p] for p in P_vals]
    plt.plot(P_vals, E_vals, marker='o', linewidth=2, label=f'N={N} elements')

plt.title('Efficiency vs Number of Processes (Q3: Bitonic Sort)')
plt.xlabel('Number of Processes (P)')
plt.ylabel('Efficiency (Speedup / P)')
plt.xticks(sorted(list(efficiencies.values())[0].keys()))
plt.ylim(0, 1.1)
plt.grid(True, linestyle='--', alpha=0.7)
plt.legend()
plt.savefig('efficiency_plot.png', dpi=300)
print("SUCCESS: Saved efficiency_plot.png")
