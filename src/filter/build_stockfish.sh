#!/bin/bash

cd ..
make -j profile-build ARCH=x86-64-bmi2
mv stockfish ./filter/stockfish-filter-multipv2-eval-diff
echo Modified stockfish at: stockfish-filter-multipv2-eval-diff
