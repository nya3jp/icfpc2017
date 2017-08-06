proto_library(
  name = "scorer_proto",
  srcs = ["scorer.proto"],
)

cc_proto_library(
  name = "scorer_cc_proto",
  deps = ["scorer_proto"],
  visibility = ["//visibility:public"]
)

cc_library(
  name = "common",
  srcs = [
    "game_data.cc",
    "protocol.cc",
  ],
  hdrs = [
    "game_data.h",
    "protocol.h",
  ],
  deps = [
    "//third_party/chromiumbase",
  ],
  visibility = ["//visibility:public"]
)
