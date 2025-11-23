#!/bin/bash

exec >run.log 2>&1
set -euo pipefail

# -------- fixed thread list --------
threads=(1 16 32 64 128 256 384) # for a many-core server
# threads=(1 4 8 12 16 24 32) # for a laptop

# guard against CRLF endings
if grep -q $'\r' "$0"; then
  echo "ERROR: Script has Windows CRLF endings. Run: sed -i 's/\r$//' $0" >&2
  exit 1
fi

# project root
cd "$(dirname "$0")/.." || { echo "Failed to navigate to project root"; exit 1; }

DATA_DIR="$(pwd)/Data"
SCOT_DIR="SCOT"
mkdir -p "$DATA_DIR"

# clear category output dirs
categories=("listlf" "listwf" "tree")
for category in "${categories[@]}"; do
  outdir="$DATA_DIR/${category}_output_results"
  mkdir -p "$outdir"
  rm -rf "$outdir"/* || true
done

# build
cd "$SCOT_DIR" || { echo "Failed to enter $SCOT_DIR"; exit 1; }
if make -n clean &>/dev/null; then make clean; else echo "Skipping make clean (no target)"; fi
make all
cd ..

commands=(
    './SCOT/bench listlf 10 512 1 50 25 25 EBR'
    './SCOT/bench listlf 10 512 1 50 25 25 HP'
    './SCOT/bench listlf 10 512 1 50 25 25 HPO'
    './SCOT/bench listlf 10 512 1 50 25 25 IBR'
    './SCOT/bench listlf 10 512 1 50 25 25 HE'
    './SCOT/bench listlf 10 512 1 50 25 25 HYALINE'
    './SCOT/bench listlf 10 10000 1 50 25 25 EBR'
    './SCOT/bench listlf 10 10000 1 50 25 25 HP'
    './SCOT/bench listlf 10 10000 1 50 25 25 HPO'
    './SCOT/bench listlf 10 10000 1 50 25 25 IBR'
    './SCOT/bench listlf 10 10000 1 50 25 25 HE'
    './SCOT/bench listlf 10 10000 1 50 25 25 HYALINE'
    './SCOT/bench listwf 10 512 1 50 25 25 EBR'
    './SCOT/bench listwf 10 512 1 50 25 25 HP'
    './SCOT/bench listwf 10 512 1 50 25 25 HPO'
    './SCOT/bench listwf 10 512 1 50 25 25 IBR'
    './SCOT/bench listwf 10 512 1 50 25 25 HE'
    './SCOT/bench listwf 10 512 1 50 25 25 HYALINE'
    './SCOT/bench listwf 10 10000 1 50 25 25 EBR'
    './SCOT/bench listwf 10 10000 1 50 25 25 HP'
    './SCOT/bench listwf 10 10000 1 50 25 25 HPO'
    './SCOT/bench listwf 10 10000 1 50 25 25 IBR'
    './SCOT/bench listwf 10 10000 1 50 25 25 HE'
    './SCOT/bench listwf 10 10000 1 50 25 25 HYALINE'
    './SCOT/bench tree 10 128 1 50 25 25 EBR'
    './SCOT/bench tree 10 128 1 50 25 25 HP'
    './SCOT/bench tree 10 128 1 50 25 25 HPO'
    './SCOT/bench tree 10 128 1 50 25 25 IBR'
    './SCOT/bench tree 10 128 1 50 25 25 HE'
    './SCOT/bench tree 10 128 1 50 25 25 HYALINE'
    './SCOT/bench tree 10 100000 1 50 25 25 EBR'
    './SCOT/bench tree 10 100000 1 50 25 25 HP'
    './SCOT/bench tree 10 100000 1 50 25 25 HPO'
    './SCOT/bench tree 10 100000 1 50 25 25 IBR'
    './SCOT/bench tree 10 100000 1 50 25 25 HE'
    './SCOT/bench tree 10 100000 1 50 25 25 HYALINE'
    './SCOT/bench listlf 10 512 1 50 25 25 NR'
    './SCOT/bench listlf 10 10000 1 50 25 25 NR'
    './SCOT/bench listwf 10 512 1 50 25 25 NR'
    './SCOT/bench listwf 10 10000 1 50 25 25 NR'
    './SCOT/bench tree 10 128 1 50 25 25 NR'
    './SCOT/bench tree 10 100000 1 50 25 25 NR'
)

run_id="$(date +%Y%m%d_%H%M%S)"
tmp_root="./run_${run_id}"
mkdir -p "$tmp_root"

trim(){ sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//'; }

# pull the final CSV block (header + rows)
extract_csv_block(){
  awk '/^Threads, /{cap=1; print; next}
       cap && NF{print; next}
       cap && !NF{cap=0; exit}' "$1"
}

# extract lines per run as CSV: "<ClassName>,<numThreads>,<ops>,<mem>"
extract_run_rows(){
  awk '
    /^##### / {
      cls=$0
      sub(/^##### /,"",cls); sub(/ #####.*$/,"",cls)
      gsub(/  +$/,"",cls)
      cur_class=cls
    }
    /^----- Benchmark=/ {
      nt=$0
      sub(/.*numThreads=/,"",nt)
      sub(/[[:space:]].*/,"",nt)
      cur_threads=nt
      mode=1
      next
    }
    mode==1 && /^Ops\/sec = / {
      op=$0; sub(/Ops\/sec = /,"",op); gsub(/[[:space:]]/,"",op)
      cur_ops=op+0
      next
    }
    mode==1 && /^memory_usage \(Bytes\) = / {
      me=$0; sub(/memory_usage \(Bytes\) = /,"",me); gsub(/[[:space:]]/,"",me)
      cur_mem=me+0
      print cur_class "," cur_threads "," cur_ops "," cur_mem
      mode=0
    }
  ' "$1"
}

median_of_list(){ sort -n | awk '{a[NR]=$1} END{ if(NR==0){print 0; exit}; mid=int((NR+1)/2); print a[mid]; }'; }
min_of_list(){ sort -n | head -n1; }
max_of_list(){ sort -n | tail -n1; }

for cmd in "${commands[@]}"; do
  # parse: path category testlen elems runs read insert delete scheme [threads?]
  read -r bench_path category test_length element_size num_runs read_percent insert_percent delete_percent reclamation _maybe <<<"$cmd"

  # sanitize
  read_percent=${read_percent%%%}
  insert_percent=${insert_percent%%%}
  delete_percent=${delete_percent%%%}

  write_percent=$((insert_percent + delete_percent))
  rw_folder="${read_percent}R_${write_percent}W"
  elem_folder="KeyRange_${element_size}"
  output_dir="$DATA_DIR/${category}_output_results/$rw_folder/$elem_folder"
  mkdir -p "$output_dir"

  filename="${category}_${test_length}_${element_size}_${num_runs}_${read_percent}_${insert_percent}_${delete_percent}_${reclamation}.txt"
  output_file="$output_dir/$filename"
  [ -f "$output_file" ] && rm -f "$output_file"

  runs="$num_runs"
  base_cmd="./SCOT/bench $category $test_length $element_size 1 $read_percent $insert_percent $delete_percent $reclamation"

  # discovery to get header + classes
  dlog="$tmp_root/discovery_${category}_${element_size}_${reclamation}.log"
  eval "$base_cmd 1" > "$dlog" 2>&1 || true
  dcsv="$tmp_root/discovery_${category}_${element_size}_${reclamation}.csv"
  extract_csv_block "$dlog" > "$dcsv"

  header="$(head -n1 "$dcsv")"
  if [[ -z "$header" ]]; then
    echo "ERROR: Could not find CSV header in discovery output (check bench formatting)." >&2
    exit 1
  fi
  IFS=',' read -r -a cols <<<"$header"; unset IFS

  # number of classes per DS type
  if [[ "$category" == "tree" ]]; then classSize=1; else classSize=2; fi
  has_mem=1
  [[ "$reclamation" == "NR" ]] && has_mem=0

  classes=()
  for ((ci=1; ci<=classSize; ci++)); do
    classes+=("$(echo "${cols[$ci]}" | trim)")
  done

  # accumulate medians for final CSV
  declare -A FINAL_OPS_MED FINAL_MEM_MED

  {
    # ---- thread-major: for each thread, run 1..num_runs, then MEDIAN ----
    for t in "${threads[@]}"; do

      [[ "$t" =~ ^[0-9]+$ ]] || continue

      declare -A OPS_BUCKET MEM_BUCKET
      for cls in "${classes[@]}"; do OPS_BUCKET["$cls"]=""; MEM_BUCKET["$cls"]=""; done

      per_thread_cmd="$base_cmd $t"
      for ((i=1;i<=runs;i++)); do
        log="$tmp_root/t${t}_run_${i}_${category}_${element_size}_${reclamation}.log"
        eval "$per_thread_cmd" > "$log" 2>&1

        # capture numbers
        while IFS=',' read -r c tt o m; do
          c="$(echo "$c" | trim)"; tt="$(echo "$tt" | trim)"; o="$(echo "$o" | trim)"; m="$(echo "$m" | trim)"
          [[ "$tt" != "$t" ]] && continue
          OPS_BUCKET["$c"]+="$o"$'\n'
          MEM_BUCKET["$c"]+="$m"$'\n'
        done < <(extract_run_rows "$log")
      done

      # print per-class runs then medians
      for cls in "${classes[@]}"; do
        echo "##### ${cls} #####  "
        echo

        mapfile -t ops_arr < <(printf "%s" "${OPS_BUCKET[$cls]-}" | sed '/^$/d')
        mapfile -t mem_arr < <(printf "%s" "${MEM_BUCKET[$cls]-}" | sed '/^$/d')

        for ((i=1;i<=runs;i++)); do
          echo "#### RUN ${i} RESULT: ####"
          echo
          echo "----- Benchmark=${cls}   numElements=${element_size}   numThreads=${t}   testLength=${test_length}s -----"
          echo "Ops/sec = ${ops_arr[$((i-1))]:-0}"
          if [[ "$has_mem" -eq 1 ]]; then
            echo "memory_usage (Bytes) = ${mem_arr[$((i-1))]:-0}"
          else
            echo "memory_usage (Bytes) = 0"
          fi
          echo
        done

        # medians/min/max/delta
        ops_median="$(printf "%s\n" "${ops_arr[@]}" | median_of_list)"
        ops_min="$(printf "%s\n" "${ops_arr[@]}" | min_of_list)"
        ops_max="$(printf "%s\n" "${ops_arr[@]}" | max_of_list)"
        if [[ "$ops_median" == "0" ]]; then ops_delta=0; else
          ops_delta=$(awk -v max="$ops_max" -v min="$ops_min" -v med="$ops_median" 'BEGIN{printf("%ld", (100.0*(max-min)/med))}')
        fi

        mem_median=0; mem_min=0; mem_max=0; mem_delta=0
        if [[ "$has_mem" -eq 1 ]]; then
          mem_median="$(printf "%s\n" "${mem_arr[@]:-0}" | median_of_list)"
          mem_min="$(printf "%s\n" "${mem_arr[@]:-0}" | min_of_list)"
          mem_max="$(printf "%s\n" "${mem_arr[@]:-0}" | max_of_list)"
          if [[ "$mem_median" == "0" ]]; then
            mem_delta=0
          else
            mem_delta=$(awk -v max="$mem_max" -v min="$mem_min" -v med="$mem_median" 'BEGIN{printf("%ld", (100.0*(max-min)/med))}')
          fi
        fi

        echo "###### MEDIAN RESULT FOR ALL ${runs} RUNS: ######"
        echo
        echo "----- Benchmark=${cls}   numElements=${element_size}   numThreads=${t}   testLength=${test_length}s -----"
        echo "Ops/sec = ${ops_median}   delta = ${ops_delta}%   min = ${ops_min}   max = ${ops_max}"
        if [[ "$has_mem" -eq 1 ]]; then
          echo "memory_usage = ${mem_median}   delta = ${mem_delta}%   min = ${mem_min}   max = ${mem_max}"
        else
          echo "memory_usage = 0   delta = 0%   min = 0   max = 0"
        fi
        echo

        FINAL_OPS_MED["$t|$cls"]="$ops_median"
        FINAL_MEM_MED["$t|$cls"]="$mem_median"
      done
    done

    # final CSV with medians per class/thread (for our fixed thread list)
    echo
    echo "FINAL RESULTS (FOR CHARTS):"
    echo
    echo "Results in ops per second for numRuns=${runs},  length=${test_length}s "
    echo
    echo "Number of elements: ${element_size}"
    echo
    echo "$header"

    for t in "${threads[@]}"; do
      line="${t}, "
      for cls in "${classes[@]}"; do
        line+="${FINAL_OPS_MED["$t|$cls"]-0}, "
      done
      if [[ "$has_mem" -eq 1 ]]; then
        for cls in "${classes[@]}"; do
          line+="${FINAL_MEM_MED["$t|$cls"]-0}, "
        done
      fi
      echo "$line"
    done
    echo
  } | tee "$output_file"

done
