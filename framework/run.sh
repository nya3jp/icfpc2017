#!/bin/bash
exec bazel-bin/punter/punter --name "a" --punter="GreedyPunter" "$@" # --logtostderr
