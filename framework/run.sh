#!/bin/bash
exec bazel-bin/framework/game_main --name "${PUNTER_NAME}" --punter="${PUNTER_TYPE}" --logtostderr
