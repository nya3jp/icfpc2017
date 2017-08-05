#!/bin/bash

cd "$(dirname "$0")"

hash=$(git rev-parse HEAD)
rev=$(( $(ls releases | sort -n | tail -n 1 | sed 's#\..*##') + 1 ))

echo "$hash $rev"

mkdir -p release/src
git archive --format=tar HEAD . | tar x -C release/src

(
  cd release/src
  rm -rf events.inf.ed.ac.uk release releases
)

(
  cd release
  tar cvzf ../releases/$rev.tar.gz .
)

echo "saved to releases/$rev.tar.gz"
