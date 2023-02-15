#!/bin/bash

input_binpack=$1

options="
uci
setoption name PruneAtShallowDepth value false
setoption name Use NNUE value true
setoption name Threads value 1
setoption name Hash value 1024
isready
transform rescore \
  filter_depth 6 filter_multipv 2 \
  input_file ${input_binpack}
quit"

printf "$options" | ./stockfish-output-positions-csv
