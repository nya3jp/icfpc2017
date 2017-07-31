libchromiumbase
===============

This is a subset of Chromium base library as of 59.0.3071.135.

Blacklist
---------

Generally, following files are excluded because they tend to pull huge amount of dependencies:

- base/base_switches.h
- base/command_line.h
- base/metrics/...
- base/task_scheduler/...
- base/trace_event/...
