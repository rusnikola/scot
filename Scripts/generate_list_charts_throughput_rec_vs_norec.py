import os
import csv
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from io import StringIO
import cairosvg

REC_DIR = '../Data/list_output_results'
NOREC_DIR = '../Data/listnorec_output_results'
CHART_DIR = '../Data/list_charts_rec_vs_norec'
SCHEMES = ['HP', 'HE', 'IBR', 'HYALINE']

COLOR_MAP = {
    'HP': '#1f77b4',      # Blue
    'HE': '#d62728',      # Red
    'IBR': '#2ca02c',     # Green
    'HYALINE': '#ff7f0e'  # Orange
}
HATCH_MAP = {
    'norec': '//',
    'rec': '++'
}
SAVE_SUFFIX = '_rec_vs_norec_harrislist'

def extract_harris_list_column(filepath):
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
    df['Threads'] = df['Threads'].astype(int)
    df['HarrisLinkedList'] = df.iloc[:, 2].astype(int)
    return df['Threads'], df['HarrisLinkedList']


for rw_dir in os.listdir(REC_DIR):
    rec_rw_path = os.path.join(REC_DIR, rw_dir)
    norec_rw_path = os.path.join(NOREC_DIR, rw_dir)
    if not os.path.isdir(rec_rw_path): continue

    for kr_dir in os.listdir(rec_rw_path):
        if not kr_dir.startswith("KeyRange_"): continue

        rec_kr_path = os.path.join(rec_rw_path, kr_dir)
        norec_kr_path = os.path.join(norec_rw_path, kr_dir)
        if not os.path.isdir(rec_kr_path) or not os.path.isdir(norec_kr_path): continue

        fig, ax = plt.subplots(figsize=(48, 12))
        thread_vals = []
        bar_groups = []
        labels = []

        for scheme in SCHEMES:
            for variant, base_path in [('norec', norec_kr_path), ('rec', rec_kr_path)]:
                matches = [f for f in os.listdir(base_path) if f.endswith(f"_{scheme}.txt")]
                if not matches:
                    print(f"No file ending with _{scheme}.txt found in {base_path}")
                    continue

                filepath = os.path.join(base_path, matches[0])
                try:
                    threads, values = extract_harris_list_column(filepath)
                    if not thread_vals:
                        thread_vals = threads.tolist()
                    bar_groups.append(values.tolist())
                    labels.append(f"{scheme} {'w/ recovery' if variant == 'rec' else 'w/o recovery'}")
                except Exception as e:
                    print(f"Error processing {filepath}: {e}")

        if not bar_groups: continue

        # Spacing layout: tight pairs, small group gap like original script
        bar_width = 0.1
        pair_spacing = 0.1  # slight gap between rec/norec
        group_spacing = 0.1  # space between HP/HE/IBR/HYALINE within one thread group
        num_pairs = len(SCHEMES)
        total_group_width = num_pairs * (2 * bar_width + pair_spacing) + group_spacing

        index = np.arange(len(thread_vals)) * total_group_width

        for i, values in enumerate(bar_groups):
            scheme_index = i // 2  # 0: HP, 1: HE, ...
            pair_index = scheme_index
            offset = pair_index * (2 * bar_width + pair_spacing)
            if i % 2 == 1:
                offset += bar_width

            scheme = SCHEMES[scheme_index]
            variant = 'rec' if i % 2 == 1 else 'norec'

            ax.bar(index + offset, values, bar_width,
                   color=COLOR_MAP[scheme],
                   hatch=HATCH_MAP[variant],
                   edgecolor='black')

        # Tick + label formatting
        ax.set_xlabel('Threads', fontsize=36, fontweight='bold')
        ax.set_ylabel('Throughput, ops/sec', fontsize=36, fontweight='bold', labelpad=25)
        ax.set_xticks(index + (num_pairs * (bar_width + pair_spacing)) / 2)
        ax.set_xticklabels(thread_vals, fontsize=40, fontweight='bold')
        ax.ticklabel_format(axis='y', style='sci', scilimits=(0, 0), useMathText=True)
        ax.yaxis.offsetText.set_fontsize(40)
        ax.yaxis.offsetText.set_fontweight('bold')

        for tick in ax.get_yticklabels():
            tick.set_fontsize(40)
            tick.set_fontweight('bold')

        # Legend is omitted on purpose

        # Save outputs
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

