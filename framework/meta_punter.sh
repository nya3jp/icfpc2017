#!/bin/bash
exec bazel-bin/punter/punter --name "meta" --punter="MetaPunter" \
     --primary_worker 'bazel-bin/punter/punter --logtostderr --punter=GreedyPunterMirac --name=GreedyPunterMirac' \
     --backup_worker 'bazel-bin/punter/punter --logtostderr --punter=QuickPunter --name=QuickPunter' \
     "$@" # --logtostderr
