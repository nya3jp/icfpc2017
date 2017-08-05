cc_library(
  name = "random_punter",
  srcs = [
    "random_punter.cc",
  ],
  hdrs = [
    "random_punter.h",
  ],

  deps = [
    "//third_party/chromiumbase",
    "//framework:simple_punter",
  ],
  visibility = ["//visibility:public"]
)

cc_library(
  name = "greedy_punter",
  srcs = [
    "greedy_punter.cc",
  ],
  hdrs = [
    "greedy_punter.h",
  ],

  deps = [
    "//third_party/chromiumbase",
    "//framework:simple_punter",
  ],
  visibility = ["//visibility:public"]
)

cc_library(
  name = "greedy_punter_chun",
  srcs = [
    "greedy_punter_chun.cc",
    "gamemapforai.cc",
  ],
  hdrs = [
    "greedy_punter_chun.h",
    "gamemapforai.h"
  ],
  deps = [
    "//third_party/chromiumbase",
    "//framework:simple_punter",
  ],
  visibility = ["//visibility:public"]
)

proto_library(
  name = "extension_example_proto",
  srcs = ["extension_example.proto"],
  deps = [ "//framework:game_proto_proto" ],
)

cc_proto_library(
  name = "extension_example_cc_proto",
  deps = ["extension_example_proto"],
)

cc_library(
  name = "extension_example_punter",
  srcs = ["extension_example_punter.cc"],
  hdrs = ["extension_example_punter.h"],
  deps = [
    ":extension_example_cc_proto",
    "//framework:simple_punter",
    "//third_party/chromiumbase",
  ],
  visibility = ["//visibility:public"]
)

cc_library(
  name = "greedy_to_jam",
  srcs = [
    "greedy_to_jam.cc",
    "gamemapforai.cc",
  ],
  hdrs = [
    "greedy_to_jam.h",
    "gamemapforai.h"
  ],
  deps = [
    "//third_party/chromiumbase",
    "//framework:simple_punter",
  ],
  visibility = ["//visibility:public"]
)
