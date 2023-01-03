#!/bin/bash

for input_binpack in $(ls -1 ./leela99gb-data-pieces/*.binpack); do
output_binpack=./filtered/$(basename $input_binpack).filtered.binpack
echo $input_binpack
echo $output_binpack
options="
uci
setoption name PruneAtShallowDepth value false
setoption name Use NNUE value true
setoption name Threads value 95
setoption name Hash value 100000
isready
transform filter \
  filter_depth 6 filter_multipv 2 \
  input_file ${input_binpack} \
  output_file ${output_binpack}
quit"

if [ -f $output_binpack ]; then
  echo $output_binpack already exists. Skipping
else
  printf "$options" | taskset -c 0-94 ./stockfish-filter-multipv2-eval-diff
fi
done
