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
    "//third_party/chromiumbase",
    ":game",
  ]
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
  ],
  visibility = ["//visibility:public"]
)

cc_binary(
  name = "game_main",
  srcs = [
    "game_main.cc",
  ],

  deps = [
    "//third_party/chromiumbase",
    ":game",
    ":pass_punter",
    ":simple_punter",
    "//punter:random_punter",
    "//punter:greedy_punter",
  ],
)
