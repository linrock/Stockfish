#!/bin/bash
if [ "$#" -ne 2 ]; then
  echo "Usage: ./many_binpacks_to_csv.sh <input_binpack_dir> <output_csv_dir>"
  exit 0
fi

function binpack_to_csv() {
  output_filename=$(basename $1).csv
  output_dir=$2
  echo "Filtering... $1 -> $output_dir/$output_filename"
  ./binpack_to_csv.sh $1 | grep "d6 pv2" > $output_dir/$output_filename
}
export -f binpack_to_csv

# Converts binpacks to csv with search eval data for each position
concurrency=$(( $(nproc) - 1 ))
ls -1v $1/*.binpack | xargs -P $concurrency -I{} bash -c 'binpack_to_csv "$@"' _ {} $2
