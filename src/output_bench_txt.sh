#!/bin/bash
set -eu -o pipefail

make -j build
./stockfish bench | grep -v info > bench.txt
