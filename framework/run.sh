#!/bin/bash
exec bazel-bin/framework/game_main --name "a" --punter="GreedyPunter" "$@" # --logtostderr
