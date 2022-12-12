#!/bin/bash

# for i in {300..999}; do
rm -f /tmp/master_dataset_900.binpack
input_binpack="data-pieces/master_dataset_900.binpack"
output_binpack="/tmp/master_dataset_900.binpack"
options="
uci
setoption name PruneAtShallowDepth value false
setoption name Use NNUE value true
setoption name Threads value 95
setoption name Hash value 50000
isready
transform rescore depth 100 keep_moves 0 input_file ${input_binpack} output_file ${output_binpack}
quit"
if [ -f $output_binpack ]; then
  echo $output_binpack already exists. Skipping
else
  printf "$options" | taskset -c 0-94 ./stockfish | \
    tee data-pieces-wrong-nnue/stockfish-out-$i.txt
fi
# done

# stockfish transform rescore depth 9 input_file ${input_binpack} output_file ${output_binpack}

# ls -lth $output_binpack
# stockfish convert $output_binpack one-position.rescored.plain
