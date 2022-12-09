#!/bin/bash

input_plain=some-positions.plain
input_binpack=some-positions.binpack
output_binpack=some-positions.rescored.binpack
output_plain=some-positions.rescored.plain

rm -f $input_binpack $output_binpack
./stockfish convert $input_plain $input_binpack
options="
uci
setoption name PruneAtShallowDepth value false
setoption name Use NNUE value true
setoption name Threads value 1
setoption name Hash value 8
isready
transform rescore depth 9 keep_moves 0 input_file ${input_binpack} output_file ${output_binpack}
quit"
printf "$options" | ./stockfish
# stockfish transform rescore depth 9 input_file ${input_binpack} output_file ${output_binpack}

ls -lth $output_binpack
./stockfish convert $output_binpack $output_plain

diff --color $input_plain $output_plain
