import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import os

output_dir = "../Data/list_charts_rec_vs_norec"
os.makedirs(output_dir, exist_ok=True)

# Define color and hatch maps matching the chart script
COLOR_MAP = {
    'HP': '#1f77b4',      # Blue
    'HE': '#d62728',      # Red
    'IBR': '#2ca02c',     # Green
    'HYALINE': '#ff7f0e'  # Orange
}
HATCH_MAP = {
    'norec': '//',        # w/o recovery
    'rec': '++'           # w/ recovery
}

labels = [
    "HP (w/o recovery)", "HP (w/ recovery)",
    "HE (w/o recovery)", "HE (w/ recovery)",
    "IBR (w/o recovery)", "IBR (w/ recovery)",
    "HLN (w/o recovery)", "HLN (w/ recovery)"
]

colors = []
hatches = []
for scheme in ['HP', 'HE', 'IBR', 'HYALINE']:
    colors.append(COLOR_MAP[scheme])
    hatches.append(HATCH_MAP['norec'])
    colors.append(COLOR_MAP[scheme])
    hatches.append(HATCH_MAP['rec'])

handles = []
for i, label in enumerate(labels):
    patch = mpatches.Patch(
        facecolor=colors[i],
        hatch=hatches[i],
        edgecolor='black',
        label=label
    )
    handles.append(patch)

fig, ax = plt.subplots(figsize=(16, 16))
ax.axis('off')

legend = ax.legend(
    handles=handles,
    labels=labels,
    loc='center',
    ncol=1,
    fontsize=40,
    frameon=True,
    fancybox=False,
    handletextpad=2,
    columnspacing=2,
    edgecolor='black'
)

frame = legend.get_frame()
frame.set_linestyle('--')
frame.set_linewidth(1.5)

for text in legend.get_texts():
    text.set_fontsize(40)
    text.set_fontweight('bold')

for ext in ['svg', 'pdf']:
    save_path = os.path.join(output_dir, f"list_rec_vs_norec_legend.{ext}")
    plt.savefig(save_path, format=ext, bbox_inches='tight')
    print(f"Saved legend: {save_path}")

plt.close()

