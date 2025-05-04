import os
import csv
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import cairosvg

BASE_DIR = '../Data/tree_output_results'
CHART_DIR = '../Data/tree_charts'

LINE_STYLES = ['-', '--', '-.', ':']
MARKERS = ['o', 's', 'D', '^', 'v', 'p', '*']
COLORS = ['m', 'c', 'blue', 'red', 'green', 'purple', 'orange']

column_replacements = {
    "NatarajanMittalTreeEBR_Memory_Usage": "NMTree-EBR",
    "NatarajanMittalTreeHP_Memory_Usage": "NMTree-HP",
    "NatarajanMittalTreeIBR_Memory_Usage": "NMTree-IBR",
    "NatarajanMittalTreeHE_Memory_Usage": "NMTree-HE"
}

def extract_tree_memory_data(files, path):
    thread_vals = []
    data_map = {}
    for fname in files:
        full_path = os.path.join(path, fname)
        with open(full_path, 'r') as f:
            lines = f.readlines()

        start = -1
        for i, line in enumerate(lines):
            if line.strip().startswith("Number of elements:"):
                for j in range(i + 1, len(lines)):
                    if lines[j].strip().startswith("Threads"):
                        start = j
                        break
                break

        if start == -1:
            continue

        header = [h.strip() for h in lines[start].split(',')]
        if any(x in header[1] for x in ["NR", "HYALINE"]):
            continue

        for line in lines[start+1:]:
            if not line.strip():
                break
            values = [v.strip() for v in line.strip().split(',')]
            if len(values) < 3:
                continue
            threads = int(values[0])
            mem = int(values[2])  # 3rd column is memory
            label = column_replacements.get(header[2])
            if label:
                data_map.setdefault(label, []).append(mem)
                if threads not in thread_vals:
                    thread_vals.append(threads)
    return thread_vals, data_map

def plot_memory_chart(thread_vals, data_map, output_dir, filename_prefix):
    fig, ax = plt.subplots(figsize=(16, 11))
    labels = sorted(data_map.keys())
    for i, label in enumerate(labels):
        ax.plot(
            thread_vals, data_map[label],
            linestyle=LINE_STYLES[i % len(LINE_STYLES)],
            marker=MARKERS[i % len(MARKERS)],
            markersize=10,
            linewidth=2.5,
            label=label,
            color=COLORS[i % len(COLORS)]
        )

    ax.set_xlabel('Threads', fontsize=36, fontweight='bold', labelpad=5)
    ax.set_ylabel('Not-Yet-Reclaimed Objects', fontsize=36, fontweight='bold', labelpad=25)
    ax.set_xticks(thread_vals)
    custom_labels = [str(t) if t != 16 else '' for t in thread_vals]  # Hide label for 16
    ax.set_xticklabels(custom_labels, fontsize=40, fontweight='bold')
    #ax.set_xticklabels(thread_vals, fontsize=40, fontweight='bold')
    ax.ticklabel_format(axis='y', style='sci', scilimits=(0, 0), useMathText=True)
    ax.yaxis.offsetText.set_fontsize(40)
    ax.yaxis.offsetText.set_fontweight('bold')
    for tick in ax.get_yticklabels():
        tick.set_fontsize(40)
        tick.set_fontweight('bold')
        
    legend = ax.legend(prop={'size': 35, 'weight': 'bold'}, frameon=True, fancybox=False, edgecolor='black', handletextpad=1.4)

    # Customize legend frame
    frame = legend.get_frame()
    frame.set_linestyle('--')
    frame.set_linewidth(1.2)

    for text in legend.get_texts():
        text.set_fontsize(35)
        text.set_fontweight('bold')

    os.makedirs(output_dir, exist_ok=True)
    svg_path = os.path.join(output_dir, f"{filename_prefix}_memory_plot.svg")
    pdf_path = svg_path.replace('.svg', '.pdf')
    for path in [svg_path, pdf_path]:
        if os.path.exists(path): os.remove(path)

    plt.savefig(svg_path, format='svg', bbox_inches='tight')
    print(f"Saved SVG: {svg_path}")
    try:
        cairosvg.svg2pdf(url=svg_path, write_to=pdf_path)
        print(f"Converted to PDF: {pdf_path}")
    except Exception as e:
        print(f"Failed to convert {svg_path} to PDF: {e}")

    plt.close(fig)

for rw_dir in os.listdir(BASE_DIR):
    for kr_dir in os.listdir(os.path.join(BASE_DIR, rw_dir)):
        path = os.path.join(BASE_DIR, rw_dir, kr_dir)
        files = [f for f in os.listdir(path) if f.endswith('.txt')]
        threads, data = extract_tree_memory_data(files, path)
        if data:
            outdir = os.path.join(CHART_DIR, rw_dir, kr_dir)
            plot_memory_chart(threads, data, outdir, f"{rw_dir}_{kr_dir}")

