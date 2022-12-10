#!/bin/bash

tests=(
  test.change-moves
  test.remove-captures
  test.remove-in-check
)
for test in ${tests[@]}; do

echo $test
input_plain=$test.plain
input_binpack=$test.binpack
output_binpack=$test.rescored.binpack
output_plain=$test.rescored.plain

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

ls -lth $output_binpack
./stockfish convert $output_binpack $output_plain

diff --color $input_plain $output_plain

done
