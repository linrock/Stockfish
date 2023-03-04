#!/bin/bash
set -eu -o pipefail

binpack_file=$1

if [ ! -f $binpack_file ]; then
  echo Binpack file not found: $binpack_file
  exit
fi

csv_filename=$(basename $binpack_file).csv
csv_filepath=$(dirname $binpack_file)/$csv_filename

if [ ! -f $csv_filepath ]; then
  echo CSV file not found: $csv_filepath
  exit
fi

echo "Found binpack: $binpack_file "
echo "Found CSV:     $csv_filepath"
echo Counting binpack positions...
num_binpack_positions=$(./stockfish-output-positions-csv \
  gather_statistics position_count \
  input_file $binpack_file | grep "Number of positions" | awk '{print $NF}')
num_csv_positions=$(wc -l $csv_filepath | awk '{print $1}')

echo Num binpack positions: $num_binpack_positions
echo Num csv positions: $num_csv_positions

if [ $num_binpack_positions -eq $num_csv_positions ]; then
  echo Same number of positions in both! Compressing CSV file...
  zstd --ultra -22 --rsyncable --rm $csv_filepath
else
  echo Not continuing, Different number of positions: $num_binpack_positions != $num_csv_positions
fi
