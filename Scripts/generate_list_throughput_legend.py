import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import os

output_dir = "../Data/list_charts"
os.makedirs(output_dir, exist_ok=True)

# Label list in group order: each column = one SMR scheme
labels = [
    "HMList-NR", "HList-NR",
    "HMList-EBR", "HList-EBR",
    "HMList-HP", "HList-HP (New)",
    "HMList-IBR", "HList-IBR (New)",
    "HMList-HE", "HList-HE (New)",
    "HMList-HLN", "HList-HLN (New)"
]

# Color by SMR scheme
COLOR_MAP = {
    "NR": "#9467bd",      # Purple
    "EBR": "#8c564b",     # Brown
    "HP": "#1f77b4",      # Blue
    "IBR": "#2ca02c",     # Green
    "HE": "#d62728",      # Red
    "HLN": "#ff7f0e"      # Orange
}

# Hatch by list type
HATCH_MAP = {
    "HMList": "//",
    "HList": "++"
}

# Extract color/hatch per label
colors = []
hatches = []
for label in labels:
    scheme = label.split('-')[-1].replace(" (New)", "")
    impl = "HList" if "HList" in label else "HMList"
    colors.append(COLOR_MAP[scheme])
    hatches.append(HATCH_MAP[impl])

# Create legend patches
handles = []
for i, label in enumerate(labels):
    patch = mpatches.Patch(
        facecolor=colors[i],
        hatch=hatches[i],
        edgecolor='black',
        label=label
    )
    handles.append(patch)

# Plot the legend-only figure
fig, ax = plt.subplots(figsize=(32, 4))  # Wide to fit 6 groups
ax.axis('off')

legend = ax.legend(
    handles=handles,
    labels=labels,
    loc='center',
    ncol=6,
    fontsize=40,
    frameon=True,
    fancybox=False,
    handletextpad=2,
    columnspacing=2.5,
    edgecolor='black'
)

# Style the legend box
frame = legend.get_frame()
frame.set_linestyle('--')
frame.set_linewidth(1.5)

# Bolden the text
for text in legend.get_texts():
    text.set_fontsize(40)
    text.set_fontweight('bold')

# Save as both SVG and PDF
for ext in ['svg', 'pdf']:
    save_path = os.path.join(output_dir, f"list_throughput_legend.{ext}")
    plt.savefig(save_path, format=ext, bbox_inches='tight')
    print(f"Saved legend: {save_path}")

plt.close()

