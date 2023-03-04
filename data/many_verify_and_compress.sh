#!/bin/bash
if [ "$#" -ne 1 ]; then
  echo "Usage: ./many_verify_and_compress.sh <binpack_dir>"
  exit 0
fi

# concurrency=$(( $(nproc) - 1 ))
concurrency=70
ls -1v $1/*.binpack | xargs -P $concurrency -n1 ./verify_and_compress.sh
