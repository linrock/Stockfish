#!/bin/bash
set -eu -o pipefail


nnue_filename=$1

sha256_hash=$(sha256sum $nnue_filename)
nnue_name=nn-${sha256_hash:0:12}.nnue
cp $nnue_filename $nnue_name
ls -lth $nnue_name
echo $nnue_name

nnue_file_size=$(ls -lth $nnue_name | awk '{print $5}' | grep -oE '[0-9]+')
echo nnue file size: $nnue_file_size M

git checkout master
git pull origin master
git log --oneline | head -1 || true
echo

git checkout -b $nnue_name
sed -i '' "s/NameBig \"nn-[a-f0-9]\{12\}\.nnue/NameBig \"$nnue_name/g" evaluate.h
git add -u

echo "Building stockfish..."
make -j build optimize=no >/dev/null 2>&1

echo "Getting stockfish bench value..."
nodes_searched=$(./stockfish bench 2>&1 | grep "Nodes searched" | grep -oE "\d+")

echo $nnue_name
echo bench: $nodes_searched

git commit -m "$nnue_name

bench $nodes_searched
"

# git push --set-upstream origin $nnue_name
