proto_library(
  name = "game_proto_proto",
  srcs = ["game_proto.proto"],
  deps = [
    "//common:scorer_proto",
  ],
  visibility = ["//visibility:public"]
)

cc_proto_library(
  name = "game_proto_cc_proto",
  deps = ["game_proto_proto"],
  visibility = ["//visibility:public"]
)

cc_library(
  name = "game",
  srcs = [
    "game.cc",
  ],
  hdrs = [
    "game.h",
  ],

  deps = [
    "//third_party/chromiumbase",
    "//common",
  ],
  visibility = ["//visibility:public"]
)

cc_library(
  name = "simple_punter",
  srcs = [
    "simple_punter.cc",
  ],
  hdrs = [
    "simple_punter.h",
  ],

  deps = [
    "//third_party/chromiumbase",
    ":game",
    ":game_proto_cc_proto",
  ],
  visibility = ["//visibility:public"]
)
