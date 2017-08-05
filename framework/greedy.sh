#!/bin/bash
exec bazel-bin/framework/game_main --name "greedy" --punter="GreedyPunter" "$@" # --logtostderr
