import matplotlib.pyplot as plt
import re
from collections import defaultdict
import os

# Data structures to hold parsed metrics
speedups = defaultdict(dict)
efficiencies = defaultdict(dict)

print("Reading analysis.txt...")

try:
    with open('analysis.txt', 'r') as f:
        for line in f:
            if "P=" in line and "V=" in line and "Speedup=" in line:
                try:
                    p_match = re.search(r'P=(\d+)', line)
                    v_match = re.search(r'V=(\d+)', line)
                    speedup_match = re.search(r'Speedup=([0-9\.]+)', line)
                    
                    if p_match and v_match and speedup_match:
                        P = int(p_match.group(1))
                        V = int(v_match.group(1))
                        speedup = float(speedup_match.group(1))
                        
                        speedups[V][P] = speedup
                        efficiencies[V][P] = speedup / P
                except Exception as e:
                    pass
except FileNotFoundError:
    print("Error: analysis.txt not found! Please run the SCP command to download it from the cluster first.")
    exit(1)

if not speedups:
    print("Error: No valid data found in analysis.txt")
    exit(1)

# 1. Plot Speedup
plt.figure(figsize=(10, 6))
for V, p_data in sorted(speedups.items()):
    P_vals = sorted(p_data.keys())
    S_vals = [p_data[p] for p in P_vals]
    plt.plot(P_vals, S_vals, marker='o', linewidth=2, label=f'V={V} vertices')

# Ideal Speedup Line
max_p = max([max(p_data.keys()) for p_data in speedups.values()])
plt.plot([1, max_p], [1, max_p], 'k--', label='Ideal Speedup')

plt.title('Speedup vs Number of Processes (Connected Components)')
plt.xlabel('Number of Processes (P)')
plt.ylabel('Speedup (T1 / Tp)')
plt.xticks(sorted(list(speedups.values())[0].keys()))
plt.grid(True, linestyle='--', alpha=0.7)
plt.legend()
plt.savefig('speedup_plot.png', dpi=300)
print("SUCCESS: Saved speedup_plot.png")

# 2. Plot Efficiency
plt.figure(figsize=(10, 6))
for V, p_data in sorted(efficiencies.items()):
    P_vals = sorted(p_data.keys())
    E_vals = [p_data[p] for p in P_vals]
    plt.plot(P_vals, E_vals, marker='o', linewidth=2, label=f'V={V} vertices')

plt.title('Efficiency vs Number of Processes (Connected Components)')
plt.xlabel('Number of Processes (P)')
plt.ylabel('Efficiency (Speedup / P)')
plt.xticks(sorted(list(efficiencies.values())[0].keys()))
plt.ylim(0, 1.1)
plt.grid(True, linestyle='--', alpha=0.7)
plt.legend()
plt.savefig('efficiency_plot.png', dpi=300)
print("SUCCESS: Saved efficiency_plot.png")
