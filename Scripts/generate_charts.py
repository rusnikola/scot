import subprocess
import os


SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))


chart_scripts = [
    os.path.join(SCRIPTS_DIR, "generate_list_throughput_legend.py"),
    os.path.join(SCRIPTS_DIR, "generate_list_charts_throughput.py"),
    os.path.join(SCRIPTS_DIR, "generate_list_memory_legend.py"),
    os.path.join(SCRIPTS_DIR, "generate_list_charts_memory.py"),
    os.path.join(SCRIPTS_DIR, "generate_tree_throughput_legend.py"),
    os.path.join(SCRIPTS_DIR, "generate_tree_charts_throughput.py"),
    os.path.join(SCRIPTS_DIR, "generate_tree_memory_legend.py"),
    os.path.join(SCRIPTS_DIR, "generate_tree_charts_memory.py")
]

for script in chart_scripts:
    script_name = os.path.basename(script)
    print(f"\nRunning script: {script_name}...\n")
    try:
        subprocess.run(["python3", script], check=True)
        print(f"\nCompleted {script_name}!\n")
    except subprocess.CalledProcessError as e:
        print(f"Error executing {script}: {e}")

