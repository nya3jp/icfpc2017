cc_binary(
  name = "punter",
  srcs = [
    "punter.cc",
  ],

  deps = [
    ":extension_example_punter",
    ":greedy_punter",
    ":greedy_punter_chun",
    ":greedy_punter_mirac",
    ":greedy_to_jam",
    ":lazy_punter",
    ":pass_punter",
    ":quick_punter",
    ":random_punter",
    "//framework:game",
    "//framework:simple_punter",
    "//third_party/chromiumbase",
  ],
)

cc_library(
  name = "pass_punter",
  srcs = [
    "pass_punter.cc",
  ],
  hdrs = [
    "pass_punter.h",
  ],

  deps = [
    "//framework:game",
    "//third_party/chromiumbase",
  ],
)

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
)

cc_library(
  name = "quick_punter",
  srcs = [
    "quick_punter.cc",
  ],
  hdrs = [
    "quick_punter.h",
  ],

  deps = [
    ":quick_punter_cc_proto",
    "//third_party/chromiumbase",
    "//framework:game",
  ],
)

proto_library(
  name = "quick_punter_proto",
  srcs = ["quick_punter.proto"],
  deps = [ "//framework:game_proto_proto" ],
)

cc_proto_library(
  name = "quick_punter_cc_proto",
  deps = ["quick_punter_proto"],
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
    ":greedy_punter_cc_proto",
    "//third_party/chromiumbase",
    "//framework:simple_punter",
  ],
)

proto_library(
  name = "greedy_punter_proto",
  srcs = ["greedy_punter.proto"],
  deps = [ "//framework:game_proto_proto" ],
)

cc_proto_library(
  name = "greedy_punter_cc_proto",
  deps = ["greedy_punter_proto"],
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
)

cc_library(
  name = "greedy_punter_mirac",
  srcs = [
    "greedy_punter_mirac.cc",
  ],
  hdrs = [
    "greedy_punter_mirac.h",
  ],

  deps = [
    "//third_party/chromiumbase",
    "//framework:simple_punter",
  ],
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
)

cc_library(
  name = "lazy_punter",
  srcs = [
    "lazy_punter.cc",
  ],
  hdrs = [
    "lazy_punter.h",
  ],

  deps = [
    "//third_party/chromiumbase",
    "//framework:simple_punter",
  ],
)
