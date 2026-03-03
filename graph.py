import pandas as pd
import matplotlib.pyplot as plt
import os

def main():
    policies = ['First_Fit', 'Best_Fit', 'Worst_Fit']
    colors = {'First_Fit': 'blue', 'Best_Fit': 'green', 'Worst_Fit': 'red'}

    # =========================================================================
    # GRAPH 1: Speed vs Size (Log-Log Scale)
    # =========================================================================
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    # Subplot 1: tmalloc() speed
    for policy in policies:
        filename = f'speed_{policy}.csv'
        if os.path.exists(filename):
            df = pd.read_csv(filename)
            ax1.plot(df['Size(Bytes)'], df['MallocTime(ns)'], marker='o', markersize=4, 
                     label=policy.replace('_', ' '), color=colors[policy])

    ax1.set_xscale('log', base=2)
    ax1.set_yscale('log', base=10)
    ax1.set_xlabel('Allocation Size (Bytes) [Log Scale]')
    ax1.set_ylabel('Time (ns) [Log Scale]')
    ax1.set_title('tmalloc() Speed vs. Allocation Size')
    ax1.legend()
    ax1.grid(True, which="both", ls="--", alpha=0.5)

    # Subplot 2: tfree() speed
    for policy in policies:
        filename = f'speed_{policy}.csv'
        if os.path.exists(filename):
            df = pd.read_csv(filename)
            ax2.plot(df['Size(Bytes)'], df['FreeTime(ns)'], marker='x', markersize=4, 
                     linestyle='--', label=policy.replace('_', ' '), color=colors[policy])

    ax2.set_xscale('log', base=2)
    ax2.set_yscale('log', base=10)
    ax2.set_xlabel('Allocation Size (Bytes) [Log Scale]')
    ax2.set_ylabel('Time (ns) [Log Scale]')
    ax2.set_title('tfree() Speed vs. Allocation Size')
    ax2.legend()
    ax2.grid(True, which="both", ls="--", alpha=0.5)

    plt.tight_layout()
    plt.savefig('speed_comparison.png')
    plt.clf()
    plt.close()

    # =========================================================================
    # GRAPH 2: Memory Utilization Over Time
    # =========================================================================
    fig, ax = plt.subplots(figsize=(10, 6))
    for policy in policies:
        filename = f'utilization_{policy}.csv'
        if os.path.exists(filename):
            df = pd.read_csv(filename)
            ax.plot(df['Time(ns)'], df['Utilization(%)'], alpha=0.7, 
                     label=policy.replace('_', ' '), color=colors[policy])

    ax.set_xlabel('Time (ns)')
    ax.set_ylabel('Memory Utilization (%)')
    ax.set_title('Memory Utilization Over Time')
    ax.legend()
    ax.grid(True, alpha=0.5)
    
    plt.tight_layout()
    plt.savefig('utilization_comparison.png')
    plt.clf()
    plt.close()

    # =========================================================================
    # GRAPH 3: Average Utilization Bar Chart
    # =========================================================================
    avg_util_filename = 'average_utilization.csv'
    if os.path.exists(avg_util_filename):
        try:
            df_avg = pd.read_csv(avg_util_filename, header=None, names=['Policy', 'Utilization'])
            
            # Clean up Policy names and convert Utilization string (e.g. '85.5%') to float
            df_avg['Policy'] = df_avg['Policy'].str.strip()
            df_avg['Utilization'] = df_avg['Utilization'].astype(str).str.replace('%', '').astype(float)
            
            fig, ax = plt.subplots(figsize=(7, 4))
            plot_colors = [colors.get(p, 'gray') for p in df_avg['Policy']]
            
            ax.bar(df_avg['Policy'].str.replace('_', ' '), df_avg['Utilization'], color=plot_colors, alpha=0.8)
            ax.set_xlabel('Allocation Policy')
            ax.set_ylabel('Average Memory Utilization (%)')
            ax.set_title('Average Memory Utilization by Policy')
            ax.set_ylim(0, 100) # Cap at 100 for percentages
            ax.grid(axis='y', linestyle='--', alpha=0.7)
            
            plt.tight_layout()
            plt.savefig('average_utilization_bar.png')
            plt.clf()
            plt.close()
        except Exception:
            pass

    # =========================================================================
    # GRAPH 4: Data Structure Overhead Bar Chart
    # =========================================================================
    overhead_filename = 'overhead.csv'
    if os.path.exists(overhead_filename):
        try:
            df_over = pd.read_csv(overhead_filename, header=None, names=['Policy', 'Overhead'])
            
            # Clean up Policy names and convert Overhead string (e.g. '1024 bytes') to numeric
            df_over['Policy'] = df_over['Policy'].str.strip()
            df_over['Overhead'] = df_over['Overhead'].astype(str).str.replace('bytes', '', case=False).str.strip().astype(float)
            
            # Make the figure more compact
            fig, ax = plt.subplots(figsize=(7, 4))
            plot_colors = [colors.get(p, 'gray') for p in df_over['Policy']]
            
            ax.bar(df_over['Policy'].str.replace('_', ' '), df_over['Overhead'], color=plot_colors, alpha=0.8)
            ax.set_xlabel('Allocation Policy')
            ax.set_ylabel('Data Structure Overhead (Bytes)')
            ax.set_title('Data Structure Overhead')
            ax.grid(axis='y', linestyle='--', alpha=0.7)
            
            # Zoom in on the Y-axis to show small differences clearly
            min_val = df_over['Overhead'].min()
            max_val = df_over['Overhead'].max()
            diff = max_val - min_val
            
            # If there's a difference, pad by 20% of the difference. If they are exactly the same, pad by 5%.
            padding = diff * 0.5 if diff > 0 else min_val * 0.05
            ax.set_ylim(max(0, min_val - padding), max_val + padding)
            
            plt.tight_layout()
            plt.savefig('overhead_bar.png')
            plt.clf()
            plt.close()
        except Exception:
            pass

if __name__ == "__main__":
    main()