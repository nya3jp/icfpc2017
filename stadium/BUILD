# -*- mode: python -*-

cc_binary(
  name = "stadium",
  srcs = ["stadium.cc"],
  deps = [
    ":stadium_lib",
    "//third_party/chromiumbase",
  ],
)

cc_library(
  name = "stadium_lib",
  srcs = [
    "game_data.cc",
    "local_punter.cc",
    "master.cc",
    "referee.cc",
  ],
  hdrs = [
    "game_data.h",
    "local_punter.h",
    "master.h",
    "punter.h",
    "referee.h",
  ],
  deps = [
    "//third_party/chromiumbase",
    "//common",
    "//common:scorer_cc_proto",
  ],
  copts = [
    "-Wno-sign-compare",
  ],
)
