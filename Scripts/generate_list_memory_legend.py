import matplotlib.pyplot as plt
import os

output_dir = "../Data/list_charts"
os.makedirs(output_dir, exist_ok=True)

labels = [
    "HMList-EBR", "HList-EBR",
    "HMList-HP", "HList-HP (New)",
    "HMList-IBR", "HList-IBR (New)",
    "HMList-HE", "HList-HE (New)"
]

line_styles = ['-', '--', '-.', ':']
markers = ['o', 's', 'D', '^', 'v', 'p', '*']
colors = ['m', 'c', 'blue', 'red', 'green', 'purple', 'orange', 'brown', 'pink', 'olive']

fig, ax = plt.subplots(figsize=(16, 6))
ax.axis('off')

lines = []
for i, label in enumerate(labels):
    line, = ax.plot([], [],
                    linestyle=line_styles[i % len(line_styles)],
                    marker=markers[i % len(markers)],
                    markersize=10,
                    linewidth=2.5,
                    color=colors[i % len(colors)],
                    label=label)
    lines.append(line)

legend = ax.legend(
    handles=lines,
    labels=labels,
    loc='center',
    ncol=2,
    fontsize=36,
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
    text.set_fontsize(36)
    text.set_fontweight('bold')

for ext in ['svg', 'pdf']:
    save_path = os.path.join(output_dir, f"list_memory_legend.{ext}")
    plt.savefig(save_path, format=ext, bbox_inches='tight')
    print(f"Saved list memory legend: {save_path}")

plt.close()
