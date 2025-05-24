#!/bin/bash

cd "$(dirname "$0")/.." || { echo "Failed to navigate to SPAA directory"; exit 1; }

DATA_DIR="Data"
SCOT_DIR="SCOT"

mkdir -p "$DATA_DIR"

# Define benchmark categories
categories=("list" "listnorec" "tree")

for category in "${categories[@]}"; do
    output_folder="$DATA_DIR/${category}_output_results"
    mkdir -p "$output_folder"
    rm -rf "$output_folder"/*  # Clear contents
done

cd "$SCOT_DIR" || { echo "Failed to enter $SCOT_DIR directory"; exit 1; }

if make -n clean &>/dev/null; then
    make clean
else
    echo "Skipping make clean: No 'clean' target found."
fi

make all || { echo "Make failed"; exit 1; }

cd .. || { echo "Failed to return to parent directory"; exit 1; }

# Define dynamic commands with full parameter set
commands=(
    './SCOT/bench list 10 16 5 50 25 25 EBR'
    './SCOT/bench list 10 16 5 50 25 25 HP'
    './SCOT/bench list 10 16 5 50 25 25 IBR'
    './SCOT/bench list 10 16 5 50 25 25 HE'
    './SCOT/bench list 10 16 5 50 25 25 HYALINE'
    './SCOT/bench list 10 512 5 50 25 25 EBR'
    './SCOT/bench list 10 512 5 50 25 25 HP'
    './SCOT/bench list 10 512 5 50 25 25 IBR'
    './SCOT/bench list 10 512 5 50 25 25 HE'
    './SCOT/bench list 10 512 5 50 25 25 HYALINE'
    './SCOT/bench list 10 10000 5 50 25 25 EBR'
    './SCOT/bench list 10 10000 5 50 25 25 HP'
    './SCOT/bench list 10 10000 5 50 25 25 IBR'
    './SCOT/bench list 10 10000 5 50 25 25 HE'
    './SCOT/bench list 10 10000 5 50 25 25 HYALINE'
    './SCOT/bench list 10 16 5 0 50 50 EBR'
    './SCOT/bench list 10 16 5 0 50 50 HP'
    './SCOT/bench list 10 16 5 0 50 50 IBR'
    './SCOT/bench list 10 16 5 0 50 50 HE'
    './SCOT/bench list 10 16 5 0 50 50 HYALINE'
    './SCOT/bench list 10 512 5 0 50 50 EBR'
    './SCOT/bench list 10 512 5 0 50 50 HP'
    './SCOT/bench list 10 512 5 0 50 50 IBR'
    './SCOT/bench list 10 512 5 0 50 50 HE'
    './SCOT/bench list 10 512 5 0 50 50 HYALINE'
    './SCOT/bench list 10 10000 5 0 50 50 EBR'
    './SCOT/bench list 10 10000 5 0 50 50 HP'
    './SCOT/bench list 10 10000 5 0 50 50 IBR'
    './SCOT/bench list 10 10000 5 0 50 50 HE'
    './SCOT/bench list 10 10000 5 0 50 50 HYALINE'
    './SCOT/bench list 10 16 5 90 5 5 EBR'
    './SCOT/bench list 10 16 5 90 5 5 HP'
    './SCOT/bench list 10 16 5 90 5 5 IBR'
    './SCOT/bench list 10 16 5 90 5 5 HE'
    './SCOT/bench list 10 16 5 90 5 5 HYALINE'
    './SCOT/bench list 10 512 5 90 5 5 EBR'
    './SCOT/bench list 10 512 5 90 5 5 HP'
    './SCOT/bench list 10 512 5 90 5 5 IBR'
    './SCOT/bench list 10 512 5 90 5 5 HE'
    './SCOT/bench list 10 512 5 90 5 5 HYALINE'
    './SCOT/bench list 10 10000 5 90 5 5 EBR'
    './SCOT/bench list 10 10000 5 90 5 5 HP'
    './SCOT/bench list 10 10000 5 90 5 5 IBR'
    './SCOT/bench list 10 10000 5 90 5 5 HE'
    './SCOT/bench list 10 10000 5 90 5 5 HYALINE'
    './SCOT/bench listnorec 10 512 5 50 25 25 HP'
    './SCOT/bench listnorec 10 512 5 50 25 25 IBR'
    './SCOT/bench listnorec 10 512 5 50 25 25 HE'
    './SCOT/bench listnorec 10 512 5 50 25 25 HYALINE'
    './SCOT/bench listnorec 10 10000 5 50 25 25 HP'
    './SCOT/bench listnorec 10 10000 5 50 25 25 IBR'
    './SCOT/bench listnorec 10 10000 5 50 25 25 HE'
    './SCOT/bench listnorec 10 10000 5 50 25 25 HYALINE'
    './SCOT/bench tree 10 128 5 50 25 25 EBR'
    './SCOT/bench tree 10 128 5 50 25 25 HP'
    './SCOT/bench tree 10 128 5 50 25 25 IBR'
    './SCOT/bench tree 10 128 5 50 25 25 HE'
    './SCOT/bench tree 10 128 5 50 25 25 HYALINE'
    './SCOT/bench tree 10 100000 5 50 25 25 EBR'
    './SCOT/bench tree 10 100000 5 50 25 25 HP'
    './SCOT/bench tree 10 100000 5 50 25 25 IBR'
    './SCOT/bench tree 10 100000 5 50 25 25 HE'
    './SCOT/bench tree 10 100000 5 50 25 25 HYALINE'
    './SCOT/bench tree 10 128 5 0 50 50 EBR'
    './SCOT/bench tree 10 128 5 0 50 50 HP'
    './SCOT/bench tree 10 128 5 0 50 50 IBR'
    './SCOT/bench tree 10 128 5 0 50 50 HE'
    './SCOT/bench tree 10 128 5 0 50 50 HYALINE'
    './SCOT/bench tree 10 100000 5 0 50 50 EBR'
    './SCOT/bench tree 10 100000 5 0 50 50 HP'
    './SCOT/bench tree 10 100000 5 0 50 50 IBR'
    './SCOT/bench tree 10 100000 5 0 50 50 HE'
    './SCOT/bench tree 10 100000 5 0 50 50 HYALINE'
    './SCOT/bench tree 10 128 5 90 5 5 EBR'
    './SCOT/bench tree 10 128 5 90 5 5 HP'
    './SCOT/bench tree 10 128 5 90 5 5 IBR'
    './SCOT/bench tree 10 128 5 90 5 5 HE'
    './SCOT/bench tree 10 128 5 90 5 5 HYALINE'
    './SCOT/bench tree 10 100000 5 90 5 5 EBR'
    './SCOT/bench tree 10 100000 5 90 5 5 HP'
    './SCOT/bench tree 10 100000 5 90 5 5 IBR'
    './SCOT/bench tree 10 100000 5 90 5 5 HE'
    './SCOT/bench tree 10 100000 5 90 5 5 HYALINE'
    './SCOT/bench list 10 16 1 50 25 25 NR'
    './SCOT/bench list 10 512 1 50 25 25 NR'
    './SCOT/bench list 10 10000 1 50 25 25 NR'
    './SCOT/bench list 10 16 1 0 50 50 NR'
    './SCOT/bench list 10 512 1 0 50 50 NR'
    './SCOT/bench list 10 10000 1 0 50 50 NR'
    './SCOT/bench list 10 16 1 90 5 5 NR'
    './SCOT/bench list 10 512 1 90 5 5 NR'
    './SCOT/bench list 10 10000 1 90 5 5 NR'
    './SCOT/bench tree 10 128 1 50 25 25 NR'
    './SCOT/bench tree 10 100000 1 50 25 25 NR'
    './SCOT/bench tree 10 128 1 0 50 50 NR'
    './SCOT/bench tree 10 100000 1 0 50 50 NR'
    './SCOT/bench tree 10 128 1 90 5 5 NR'
    './SCOT/bench tree 10 100000 1 90 5 5 NR'
)

for cmd in "${commands[@]}"; do
    # Extract arguments
    args=($cmd)
    category="${args[1]}"
    test_length="${args[2]}"
    element_size="${args[3]}"
    num_runs="${args[4]}"
    read_percent="${args[5]}"
    insert_percent="${args[6]}"
    delete_percent="${args[7]}"
    reclamation="${args[8]}"

    write_percent=$((insert_percent + delete_percent))
    rw_folder="${read_percent}R_${write_percent}W"
    elem_folder="KeyRange_${element_size}"

    # Construct output directory and filename
    output_dir="$DATA_DIR/${category}_output_results/$rw_folder/$elem_folder"
    mkdir -p "$output_dir"

    filename="${category}_${test_length}_${element_size}_${num_runs}_${read_percent}_${insert_percent}_${delete_percent}_${reclamation}.txt"
    output_file="$output_dir/$filename"

    # Remove file if it exists
    [ -f "$output_file" ] && rm -f "$output_file"

    echo "Running: $cmd"
    eval "$cmd 2>&1 | tee \"$output_file\""
    echo "Saved to: $output_file"
    echo "-----------------------------"
done

echo "All benchmarks completed!"
