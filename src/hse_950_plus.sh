#!/bin/bash

if [ "$#" -ne 1 ]; then
  echo "Usage: ./hse_950_plus.sh <input_binpack>"
  exit 0
fi

input_binpack=$1

options="
uci
setoption name PruneAtShallowDepth value false
setoption name Use NNUE value false
setoption name Threads value 1
setoption name Hash value 1024
isready
transform high_simple_eval \
  input_file ${input_binpack} \
  output_file ${input_binpack}.hse-950-plus.binpack
quit"

printf "$options" | ./stockfish
