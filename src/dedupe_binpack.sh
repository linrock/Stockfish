#!/bin/bash
if [ "$#" -ne 1 ]; then
  echo "Usage: ./dedupe_binpack.sh <input_binpack>"
  exit 0
fi

# rm tiny.binpack.dd.binpack

input_binpack=$1
output_binpack=${input_binpack}.dd.binpack
if [ -f $output_binpack ]; then
  echo De-duped binpack already exists: $output_binpack
  exit 0
fi

options="
uci
setoption name Threads value 1
isready
transform rescore \
  input_file ${input_binpack} \
  output_file ${output_binpack}
quit"

printf "$options" | ./stockfish

# ./stockfish convert ${output_binpack} tiny.dd.plain
