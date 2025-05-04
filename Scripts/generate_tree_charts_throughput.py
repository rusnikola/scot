import os
import csv
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import cairosvg

# CONFIG
BASE_DIR = '../Data/tree_output_results'
CHART_DIR = '../Data/tree_charts'
HATCHES = ['-', '\\', '/', '*']
COLORS = ['gold', 'g', 'm', 'c', 'blue', 'red', 'green', 'purple']
SAVE_SUFFIX = '_throughput_plot'

column_name_replacements = {
    "NatarajanMittalTreeNR": "NMTree-NR",
    "NatarajanMittalTreeEBR": "NMTree-EBR",
    "NatarajanMittalTreeHP": "NMTree-HP",
    "NatarajanMittalTreeIBR": "NMTree-IBR",
    "NatarajanMittalTreeHE": "NMTree-HE",
    "NatarajanMittalTreeHYALINE": "NMTree-HLN"
}

def benchmark_sort_key(label):
    suffix = label.split('-')[-1]
    return ['NR', 'EBR', 'HP', 'IBR', 'HE', 'HLN'].index(suffix)

def extract_table(filepath):
    with open(filepath, 'r') as f:
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
        raise ValueError(f"Could not locate throughput table in {filepath}")

    table_lines = []
    for line in lines[start:]:
        if not line.strip():
            break
        table_lines.append(line.strip())

    rows = []
    for row in csv.reader(table_lines):
        if len(row) >= 2:
            rows.append(row[:2])

    df = pd.DataFrame(rows[1:], columns=rows[0])
    df.iloc[:, 0] = df.iloc[:, 0].astype(int)
    df.iloc[:, 1] = df.iloc[:, 1].astype(int)
    return df

# MAIN
for rw_dir in os.listdir(BASE_DIR):
    rw_path = os.path.join(BASE_DIR, rw_dir)
    if not os.path.isdir(rw_path): continue

    for kr_dir in os.listdir(rw_path):
        if not kr_dir.startswith("KeyRange_"): continue
        kr_path = os.path.join(rw_path, kr_dir)
        if not os.path.isdir(kr_path): continue

        files = sorted(f for f in os.listdir(kr_path) if f.endswith('.txt'))
        if not files: continue

        thread_vals = []
        data_map = {}

        for idx, fname in enumerate(files):
            try:
                df = extract_table(os.path.join(kr_path, fname))
                if idx == 0:
                    thread_vals = df['Threads'].tolist()
                for raw_col in df.columns[1:2]:
                    label = column_name_replacements.get(raw_col.strip(), raw_col.strip())
                    data_map[label] = df[raw_col].tolist()
            except Exception as e:
                print(f"Error in {fname}: {e}")

        if not data_map: continue

        fig, ax = plt.subplots(figsize=(16, 11))

        benchmarks = sorted(data_map.keys(), key=benchmark_sort_key)
        num_benchmarks = len(benchmarks)
        bar_width = 0.15
        group_spacing = 0.3
        index = np.arange(len(thread_vals)) * (num_benchmarks * bar_width + group_spacing)

        for i, benchmark in enumerate(benchmarks):
            values = data_map[benchmark]
            offset = i * bar_width
            ax.bar(index + offset, values, bar_width,
                   label=benchmark,
                   color=COLORS[i % len(COLORS)],
                   hatch=HATCHES[i % len(HATCHES)],
                   edgecolor='black')

        ax.set_xlabel('Threads', fontsize=36, fontweight='bold', labelpad=5)
        ax.set_ylabel('Throughput, ops/sec', fontsize=36, fontweight='bold', labelpad=25)
        ax.set_xticks(index + (num_benchmarks * bar_width) / 2 - bar_width / 2)
        ax.set_xticklabels(thread_vals, fontsize=40, fontweight='bold')
        ax.ticklabel_format(axis='y', style='sci', scilimits=(0, 0), useMathText=True)
        ax.yaxis.offsetText.set_fontsize(40)
        ax.yaxis.offsetText.set_fontweight('bold')

        for tick in ax.get_yticklabels():
            tick.set_fontsize(40)
            tick.set_fontweight('bold')

        output_dir = os.path.join(CHART_DIR, rw_dir, kr_dir)
        os.makedirs(output_dir, exist_ok=True)

        svg_path = os.path.join(output_dir, f"{rw_dir}_{kr_dir}{SAVE_SUFFIX}.svg")
        pdf_path = svg_path.replace(".svg", ".pdf")

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
