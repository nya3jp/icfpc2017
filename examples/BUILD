# https://docs.bazel.build/versions/master/be/overview.html#rules

cc_binary(
  name = "minimal_main",
  srcs = [
    "minimal_main.cc",
  ],
  deps = [
    ":minimal_lib",
    "//third_party/gflags",
    "//third_party/glog",
  ],
)

cc_library(
  name = "minimal_lib",
  srcs = [
    "minimal_lib.cc",
  ],
  hdrs = [
    "minimal_lib.h",
  ],
)

cc_test(
  name = "minimal_lib_test",
  srcs = [
    "minimal_lib_test.cc",
  ],
  deps = [
    ":minimal_lib",
    "//third_party/gtest",
    "//third_party/gtest:gtest_main",
  ],
)

cc_binary(
  name = "callback_bind",
  srcs = [
    "callback_bind.cc",
  ],
  deps = [
    "//third_party/chromiumbase",
    "//third_party/gflags",
    "//third_party/glog",
  ],
)

cc_binary(
  name = "json",
  srcs = [
    "json.cc",
  ],
  deps = [
    "//third_party/chromiumbase",
    "//third_party/gflags",
    "//third_party/glog",
  ],
)

cc_binary(
  name = "worker_pool",
  srcs = [
    "worker_pool.cc",
  ],
  deps = [
    "//third_party/chromiumbase",
    "//third_party/gflags",
    "//third_party/glog",
  ],
)
