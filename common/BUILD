proto_library(
  name = "scorer_proto",
  srcs = ["scorer.proto"],
  visibility = ["//visibility:public"]
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
    "scorer.cc",
    "protocol.cc",
  ],
  hdrs = [
    "game_data.h",
    "scorer.h",
    "protocol.h",
  ],
  deps = [
    "//third_party/chromiumbase",
    ":scorer_cc_proto",
  ],
  visibility = ["//visibility:public"]
)
