import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import os

output_dir = "../Data/list_charts"
os.makedirs(output_dir, exist_ok=True)

labels = [
    "HMList-NR", "HList-NR",
    "HMList-EBR", "HList-EBR",
    "HMList-HP", "HList-HP (New)",
    "HMList-IBR", "HList-IBR (New)"
]
hatches = ['-', '\\', '/', '*']
colors = ['gold', 'g', 'm', 'c', 'blue', 'red', 'green', 'purple']

handles = []
for i, label in enumerate(labels):
    patch = mpatches.Patch(
        facecolor=colors[i],
        hatch=hatches[i % len(hatches)],
        edgecolor='black',
        label=label
    )
    handles.append(patch)

fig, ax = plt.subplots(figsize=(16, 6))
ax.axis('off')

legend = ax.legend(
    handles=handles,
    labels=labels,
    loc='center',
    ncol=2,
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
    save_path = os.path.join(output_dir, f"list_throughput_legend.{ext}")
    plt.savefig(save_path, format=ext, bbox_inches='tight')
    print(f"Saved legend: {save_path}")

plt.close()

