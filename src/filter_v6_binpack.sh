#!/bin/bash

if [ $# -ne 1 ]; then
  echo "Usage: ./filter_v6_binpack.sh <input_binpack>"
  exit 0
fi

input_binpack="$1"
output_binpack="$input_binpack.v6.binpack"

options="
uci
setoption name PruneAtShallowDepth value false
setoption name Use NNUE value true
setoption name Threads value 8
setoption name Hash value 512
isready
transform filter_v6 input_file ${input_binpack} output_file ${output_binpack}
quit"

if [ -f $output_binpack ]; then
  echo $output_binpack already exists. Skipping
else
  printf "$options" | ./stockfish
fi

# ls -lth $output_binpack
# stockfish convert $output_binpack one-position.rescored.plain
