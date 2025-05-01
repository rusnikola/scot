import matplotlib.pyplot as plt
import os

# Output directory
output_dir = "../Data/tree_charts"
os.makedirs(output_dir, exist_ok=True)

# Labels and styles (same as in list memory chart)
labels = ["NMTree-EBR", "NMTree-HP", "NMTree-IBR"]
line_styles = ['-', '--', '-.', ':']
markers = ['o', 's', 'D', '^', 'v', 'p', '*']
colors = ['m', 'c', 'blue', 'red', 'green', 'purple', 'orange']

# Create dummy lines for the legend
fig, ax = plt.subplots(figsize=(16, 4))
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

# Hide axes and only show legend
ax.set_xticks([])
ax.set_yticks([])
ax.axis('off')

legend = ax.legend(
    handles=lines,
    labels=labels,
    loc='center',
    ncol=2,
    fontsize=50,
    frameon=True,
    fancybox=False,
    handletextpad=2,
    columnspacing=2,
    edgecolor='black'
)

# Customize frame and text
frame = legend.get_frame()
frame.set_linestyle('--')
frame.set_linewidth(1.5)

for text in legend.get_texts():
    text.set_fontsize(40)
    text.set_fontweight('bold')

# Save legend
for ext in ['svg', 'pdf']:
    save_path = os.path.join(output_dir, f"tree_memory_legend.{ext}")
    plt.savefig(save_path, format=ext, bbox_inches='tight', pad_inches=0.2)
    print(f"Saved legend: {save_path}")

plt.close()
