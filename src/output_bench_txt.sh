#!/bin/bash
set -eu -o pipefail

make -j build
./stocfish bench | grep -v info > bench.txt
