#!/bin/bash
set -eu -o pipefail

python3 3072_fetch_spsa_params.py | tee spsa-variables-cpp.txt
python3 3072_apply_spsa_params.py

make clean
make -j build
echo go depth 1 | ./stockfish
./copy_nn_to_sha256.sh spsa-exported-3072.nnue
