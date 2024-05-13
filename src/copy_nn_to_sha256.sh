#!/bin/bash
# https://tests.stockfishchess.org/upload

ls -lth $1
sha256sum_out=$(sha256sum $1)
echo $sha256sum_out

new_nnue_name=nn-$(echo $sha256sum_out | grep -oE ^[0-9a-z]{12}).nnue
cp $1 $new_nnue_name
ls -lth $new_nnue_name
