#!/bin/bash

cd ../src
make -j profile-build ARCH=x86-64-bmi2
mv stockfish ../nnue-data/stockfish-output-positions-csv
echo Modified stockfish at: stockfish-output-positions-csv
