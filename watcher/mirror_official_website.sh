#!/bin/bash

cd "$(dirname "$0")/.."

set -x

while :; do
    git reset
    git clean -fd events.inf.ed.ac.uk/
    git pull
    if wget -m -p 'http://events.inf.ed.ac.uk/icfpcontest2017/'; then
        find events.inf.ed.ac.uk/ -name '*.[123456789]'  -delete
        git add events.inf.ed.ac.uk/
        if git commit -m 'Snapshot of the official site.' events.inf.ed.ac.uk/; then
            if ! git push; then
                git reset HEAD~
            fi
        fi
    fi
    sleep 1m
done
