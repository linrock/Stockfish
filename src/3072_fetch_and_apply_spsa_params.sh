#!/bin/bash
set -eu -o pipefail

python3 3072_file_twoW_fetch_spsa_params.py | tee spsa-variables-cpp.txt
python3 3072_apply_spsa_params.py
