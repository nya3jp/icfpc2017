#!/bin/bash
exec bazel-bin/punter/punter --name "greedy" --punter="GreedyPunter" "$@" # --logtostderr
