#!/bin/bash
if [ "$#" -ne 1 ]; then
  echo "Usage: ./many_binpacks_to_csv.sh <data_dir>"
  exit 0
fi

function binpack_to_csv() {
  output_filename=$(basename $1).csv
  echo "Filtering... $1 -> test80-oct2022-16tb7p-position-csv/$output_filename"
  ./binpack_to_csv.sh $1 | grep "d6 pv2" > test80-oct2022-16tb7p-position-csv/$output_filename
}
export -f binpack_to_csv

# Filters positions out of .plain files concurrently
concurrency=$(( $(nproc) - 1 ))
ls -1v $1/*.binpack | xargs -P $concurrency -I{} bash -c 'binpack_to_csv "$@"' _ {}
