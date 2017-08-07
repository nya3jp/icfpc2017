#!/bin/bash
exec bazel-bin/punter/punter --name "meta" --punter="MetaPunter" \
     --primary_worker_options '--logtostderr --punter=PassPunter --sleep_duration=2000 --name=Pass' \
     --backup_worker_options '--logtostderr --punter=QuickPunter --name=QuickPunter' \
     "$@" # --logtostderr
