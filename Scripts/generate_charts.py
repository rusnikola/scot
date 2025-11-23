import subprocess
import os
import sys

SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))

# Detect whether user passed "paper"
args = sys.argv[1:]
PAPER_MODE = ("paper" in args)

chart_scripts = [
    os.path.join(SCRIPTS_DIR, "generate_list_lf_charts_throughput.py"),
    os.path.join(SCRIPTS_DIR, "generate_list_lf_charts_memory.py"),
    os.path.join(SCRIPTS_DIR, "generate_list_wf_charts_throughput.py"),
    os.path.join(SCRIPTS_DIR, "generate_list_wf_charts_memory.py"),
    os.path.join(SCRIPTS_DIR, "generate_tree_charts_throughput.py"),
    os.path.join(SCRIPTS_DIR, "generate_tree_charts_memory.py")
]

# Scripts that should receive the "paper" flag
# (tree throughput excluded)
PAPER_AWARE = {
    "generate_list_lf_charts_throughput.py",
    "generate_list_lf_charts_memory.py",
    "generate_list_wf_charts_throughput.py",
    "generate_list_wf_charts_memory.py",
    "generate_tree_charts_memory.py"
}

for script in chart_scripts:
    script_name = os.path.basename(script)
    print(f"\nRunning script: {script_name}...\n")

    cmd = ["python3", script]

    # only forward "paper" to selected scripts
    if PAPER_MODE and script_name in PAPER_AWARE:
        cmd.append("paper")

    try:
        subprocess.run(cmd, check=True)
        print(f"\nCompleted {script_name}!\n")
    except subprocess.CalledProcessError as e:
        print(f"Error executing {script}: {e}")

