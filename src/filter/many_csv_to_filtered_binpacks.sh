#!/bin/bash
if [ "$#" -ne 2 ]; then
  echo "Usage: ./many_csv_to_filtered_binpacks.sh <csv_and_binpack_data_dir>"
  exit 0
fi

function csv_zst_to_filtered_binpack() {
  input_csv_zst_filename=$1
  output_dir=$2
  output_filtered_binpack_filename=$(basename $1).filter-v3.binpack
  cd $output_dir
  if [ -f $output_filtered_binpack_filename ]; then
    echo "Doing nothing, filtered binpack exists: $output_filtered_binpack_filename"
  else
    echo "Filtering... $input_csv_zst_filename"
    python3 csv_filter_v3.py $input_csv_zst_filename | \
      tee -a ${input_csv_zst_filename}.filter-v3.log
    stockfish convert $input_csv_zst_filename $output_filtered_binpack_filename | \
      tee -a ${input_csv_zst_filename}.filter-v3.log
    ls -lth $output_filtered_binpack_filename | \
      tee -a ${input_csv_zst_filename}.filter-v3.log
  fi
}
export -f csv_zst_to_filtered_binpack

# Converts csv.zst files into filtered binpacks
concurrency=$(( $(nproc) - 1 ))
ls -1v $1/*.csv.zst | xargs -P $concurrency -I{} bash -c 'csv_zst_to_filtered_binpack "$@"' _ {} $1
