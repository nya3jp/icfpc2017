#!/bin/bash
exec bazel-bin/punter/punter --name "greedy_chun" --punter="GreedyPunterChun" "$@" # --logtostderr
