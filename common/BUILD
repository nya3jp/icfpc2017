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
    "popen.cc",
    "protocol.cc",
    "scorer.cc",
  ],
  hdrs = [
    "game_data.h",
    "popen.h",
    "protocol.h",
    "scorer.h",
  ],
  deps = [
    "//third_party/chromiumbase",
    ":scorer_cc_proto",
  ],
  visibility = ["//visibility:public"]
)
