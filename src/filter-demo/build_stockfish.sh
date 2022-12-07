#!/bin/bash

cd ..
make -j profile-build ARCH=x86-64-modern
mv stockfish ./filter-demo/stockfish-filter
echo Modified stockfish at: stockfish-filter
