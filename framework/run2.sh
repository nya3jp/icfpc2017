#!/bin/bash
exec bazel-bin/framework/game_main --name "Chun" --punter="GreedyPunterChun" # --logtostderr
