#!/bin/bash
if [ "$#" -ne 1 ]; then
  echo "Usage: ./filter_many_pieces.sh <data_dir>"
  exit 0
fi

function filter_piece() {
  output_filename=$(basename $1).csv
  echo "Filtering... $1 -> test80-oct2022-16tb7p-position-csv/$output_filename"
  ./filter_piece.sh $1 > test80-oct2022-16tb7p-position-csv/$output_filename
}
export -f filter_piece

# Filters positions out of .plain files concurrently
ls -1v $1/*.binpack | \
  xargs -P95 -I{} bash -c 'filter_piece "$@"' _ {}
