#!/bin/bash
exec bazel-bin/framework/game_main --name "greedy_chun" --punter="GreedyPunterChun" "$@" # --logtostderr
