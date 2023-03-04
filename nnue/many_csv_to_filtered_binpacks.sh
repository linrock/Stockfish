#!/bin/bash
if [ "$#" -ne 1 ]; then
  echo "Usage: ./many_csv_to_filtered_binpacks.sh <csv_and_binpack_data_dir>"
  exit 0
fi

function csv_zst_to_filtered_binpack() {
  set -eu -o pipefail
  input_csv_zst_filename=$1
  filtered_plain_filename=${input_csv_zst_filename}.filter-v3.plain
  output_filtered_binpack_filename=${input_csv_zst_filename}.filter-v3.binpack
  filter_log_filename=${input_csv_zst_filename}.filter-v3.log
  if [ -f $output_filtered_binpack_filename ]; then
    echo "Doing nothing, filtered binpack exists: $output_filtered_binpack_filename"
  else
    echo "Filtering... $input_csv_zst_filename" | tee $filter_log_filename
    /home/ubuntu/stockfish/src/filter/env/bin/python3.11 \
      /home/ubuntu/stockfish/src/filter/csv_filter_v3.py $input_csv_zst_filename >> $filter_log_filename
    /home/ubuntu/stockfish/src/filter/stockfish-output-positions-csv \
      convert $filtered_plain_filename $output_filtered_binpack_filename >> $filter_log_filename
    ls -lth $output_filtered_binpack_filename >> $filter_log_filename
  fi
}
export -f csv_zst_to_filtered_binpack

# Converts csv.zst files into filtered binpacks
concurrency=$(( $(nproc) - 1 ))
cd $1
ls -1v *.csv.zst | head -10 | xargs -P $concurrency -I{} bash -c 'csv_zst_to_filtered_binpack "$@"' _ {}
