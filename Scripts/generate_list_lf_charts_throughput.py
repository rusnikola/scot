import os
import csv
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from io import StringIO
import cairosvg
import sys

BASE_DIR = '../Data/listlf_output_results'
CHART_DIR = '../Data/listlf_charts'
SAVE_SUFFIX = '_throughput_plot'

COLOR_MAP = {
    "NR":  "#9467bd",  # Purple
    "EBR": "#8c564b",  # Brown
    "HP": "#17becf",  # Teal
    "HPopt":  "#1f77b4",  # Blue
    "IBR": "#2ca02c",  # Green
    "HE":  "#d62728",  # Red
    "HLN": "#ff7f0e"   # Orange
}


HATCHES = {
    "HMList": "//",    # HarrisMichael
    "HList": "++"      # Harris
}


column_name_replacements = {
    "HarrisMichaelLinkedListNR": "HMList-NR",
    "HarrisLinkedListNR": "HList-NR",
    "HarrisMichaelLinkedListEBR": "HMList-EBR",
    "HarrisLinkedListEBR": "HList-EBR",
    "HarrisMichaelLinkedListHPO": "HMList-HP",
    "HarrisLinkedListHPO": "HList-HP",
    "HarrisMichaelLinkedListHP": "HMList-HPopt",
    "HarrisLinkedListHP": "HList-HPopt",
    "HarrisMichaelLinkedListIBR": "HMList-IBR",
    "HarrisLinkedListIBR": "HList-IBR",
    "HarrisMichaelLinkedListHE": "HMList-HE",
    "HarrisLinkedListHE": "HList-HE",
    "HarrisMichaelLinkedListHYALINE": "HMList-HLN",
    "HarrisLinkedListHYALINE": "HList-HLN"
}

def benchmark_sort_key(label):
    suffix = label.split('-')[-1].replace(" (New)", "")
    prefix = 0 if label.startswith("HMList") else 1
    order = ['NR', 'EBR', 'HP', 'HPopt', 'IBR', 'HE', 'HLN']
    return order.index(suffix) * 2 + prefix

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
        if len(row) >= 3:
            rows.append(row[:3])

    df = pd.DataFrame(rows[1:], columns=rows[0])
    df.iloc[:, 0] = df.iloc[:, 0].astype(int)
    df.iloc[:, 1] = df.iloc[:, 1].astype(int)
    df.iloc[:, 2] = df.iloc[:, 2].astype(int)
    return df


PAPER_MODE = ("paper" in sys.argv)

if PAPER_MODE:
    # paper mode: drop listed thread counts
    IGNORE_THREADS = {16, 384}
else:
    # normal mode: keep everything that exists in the file
    IGNORE_THREADS = set()

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
                for raw_col in df.columns[1:3]:
                    label = column_name_replacements.get(raw_col.strip(), raw_col.strip())
                    data_map[label] = df[raw_col].tolist()
            except Exception as e:
                print(f"Error in {fname}: {e}")

        if not data_map: continue


        keep_idx = [i for i, t in enumerate(thread_vals) if t not in IGNORE_THREADS]
        thread_vals = [thread_vals[i] for i in keep_idx]
        for k in list(data_map.keys()):
            data_map[k] = [data_map[k][i] for i in keep_idx]



        fig, ax = plt.subplots(figsize=(22, 10))

        benchmarks = sorted(data_map.keys(), key=benchmark_sort_key)
        num_benchmarks = len(benchmarks)
        bar_width = 0.1
        pair_spacing = 0.05
        group_spacing = 0.1
        pairs_per_group = num_benchmarks // 2
        total_group_width = pairs_per_group * (2 * bar_width + pair_spacing) + group_spacing


        index = np.arange(len(thread_vals)) * total_group_width


        for i, benchmark in enumerate(benchmarks):
            values = data_map[benchmark]
            pair_index = i // 2
            offset = pair_index * (2 * bar_width + pair_spacing)
            if i % 2 == 1:
                offset += bar_width

            scheme = benchmark.split('-')[-1].replace(" (New)", "")
            impl = "HList" if "HList" in benchmark else "HMList"

            ax.bar(index + offset, values, bar_width, label=benchmark,
                   color=COLOR_MAP[scheme],
                   hatch=HATCHES[impl],
                   edgecolor='black')


        offsets = []
        for i in range(num_benchmarks):
            pair_index = i // 2
            off = pair_index * (2 * bar_width + pair_spacing)
            if i % 2 == 1:
                off += bar_width
            offsets.append(off)

        PAD = 0.3 * bar_width

        left_edge  = (index[0]  + min(offsets)) - (bar_width / 2) - PAD
        right_edge = (index[-1] + max(offsets)) + (bar_width / 2) + PAD
        ax.set_xlim(left_edge, right_edge)


        tick_centers = index + (total_group_width - group_spacing) / 2.0
        ax.set_xticks(tick_centers)
        ax.set_xticklabels(thread_vals, fontsize=40, fontweight='bold')


        ax.set_xlabel('Threads', fontsize=36, fontweight='bold', labelpad=5)
        ax.set_ylabel('Throughput, ops/sec', fontsize=36, fontweight='bold', labelpad=25)
        ax.ticklabel_format(axis='y', style='sci', scilimits=(0, 0), useMathText=True)
        ax.yaxis.offsetText.set_fontsize(40)
        ax.yaxis.offsetText.set_fontweight('bold')
        for tick in ax.get_yticklabels():
            tick.set_fontsize(40)
            tick.set_fontweight('bold')


        legend = ax.legend(
            ncol=3, frameon=False, fancybox=False, edgecolor='black', columnspacing=0.3,  borderaxespad=0.0, labelspacing=0.2, borderpad=0.2,
            prop={'size': 32, 'weight': 'bold'}, handlelength=1.6, handletextpad=0.1
        )
        legend.get_frame().set_linestyle('--')
        legend.get_frame().set_linewidth(2)

        output_dir = os.path.join(CHART_DIR, rw_dir, kr_dir)
        os.makedirs(output_dir, exist_ok=True)

        svg_path = os.path.join(output_dir, f"{rw_dir}_{kr_dir}{SAVE_SUFFIX}.svg")
        pdf_path = os.path.join(output_dir, f"{rw_dir}_{kr_dir}{SAVE_SUFFIX}.pdf")

        for path in [svg_path, pdf_path]:
            if os.path.exists(path):
                os.remove(path)

        plt.savefig(svg_path, format='svg', bbox_inches='tight')
        print(f"Saved SVG: {svg_path}")
        plt.close(fig)

        try:
            cairosvg.svg2pdf(url=svg_path, write_to=pdf_path)
            print(f"Converted to PDF: {pdf_path}")
        except Exception as e:
            print(f"Failed to convert {svg_path} to PDF: {e}")

