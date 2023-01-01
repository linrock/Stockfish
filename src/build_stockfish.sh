#!/bin/bash

make -j profile-build ARCH=x86-64-bmi2
mv stockfish stockfish-filter-multipv2-eval-diff
