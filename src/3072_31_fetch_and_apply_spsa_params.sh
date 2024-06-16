#!/bin/bash
set -eu -o pipefail

python3 3072_31_fetch_spsa_params.py | tee spsa-variables-cpp.txt
python3 3072_31_apply_spsa_params.py
