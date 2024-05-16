#!/bin/bash
set -eu -o pipefail

python3 3072_fetch_spsa_params.py
cat spsa-variables-cpp.txt
python3 3072_apply_spsa_params.py
