#!/bin/bash
exec bazel-bin/punter/punter --name "meta" --punter="MetaPunter" \
     --primary_worker 'bazel-bin/punter/punter --logtostderr --punter=PassPunter --sleep_duration=2000 --name=Pass ' \
     --backup_worker 'bazel-bin/punter/punter --logtostderr --punter=QuickPunter --name=QuickPunter' \
     "$@" # --logtostderr
