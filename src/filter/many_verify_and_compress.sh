#!/bin/bash
if [ "$#" -ne 1 ]; then
  echo "Usage: ./many_verify_and_compress.sh <binpack_dir>"
  exit 0
fi

function binpack_to_csv() {
  input_filename=$1
  output_filename=$(basename $1).csv
  output_dir=$2
  output_csv_filepath=$output_dir/$output_filename
  if [ -f $output_csv_filepath ]; then
    echo "Doing nothing, csv exists: $output_csv_filepath"
  else
    echo "Filtering... $input_filename -> $output_csv_filepath"
    ./binpack_to_csv.sh $input_filename | grep "d6 pv2" > $output_csv_filepath
  fi
}
export -f binpack_to_csv

# concurrency=$(( $(nproc) - 1 ))
concurrency=70
ls -1v $1/*.binpack | xargs -P $concurrency -n1 ./verify_and_compress.sh
