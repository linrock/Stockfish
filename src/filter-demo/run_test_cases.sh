#!/bin/bash

tests=(
  test.remove-captures
  test.remove-in-check
)
for test in ${tests[@]}; do

echo $test
input_plain=$test.plain
input_binpack=$test.binpack
output_binpack=$test.filtered.binpack
output_plain=$test.filtered.plain

rm -f $input_binpack $output_binpack
./stockfish-filter convert $input_plain $input_binpack > /dev/null
options="
uci
setoption name PruneAtShallowDepth value false
setoption name Use NNUE value true
setoption name Threads value 1
setoption name Hash value 8
isready
transform filter depth 7 debug_print 1 input_file ${input_binpack} output_file ${output_binpack}
quit"
printf "$options" | ./stockfish-filter | grep -v "option name" | grep -v readyok | grep -v uciready

ls -lth $output_binpack
./stockfish-filter convert $output_binpack $output_plain > /dev/null
rm test.*.binpack

diff --color $input_plain $output_plain
echo

done
