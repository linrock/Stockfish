#!/bin/bash

cd ..
# make -j profile-build ARCH=x86-64-bmi2
make -j build ARCH=x86-64-bmi2
mv stockfish ./filter/stockfish-output-positions-csv
echo Modified stockfish at: stockfish-output-positions-csv
