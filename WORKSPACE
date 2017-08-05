# https://docs.bazel.build/versions/master/be/workspace.html

# Taken from https://github.com/bazelment/trunk/blob/master/WORKSPACE
new_local_repository(
  # This one can't be called protobuf because "//external:protobuf" is
  # depent by grpc
  name = "com_google_protobuf",
  path = "third_party/protobuf",
  build_file = "third_party/protobuf/BUILD",
)

new_local_repository(
  # This one can't be called protobuf because "//external:protobuf" is
  # depent by grpc
  name = "com_google_protobuf_cc",
  path = "third_party/protobuf",
  build_file = "third_party/protobuf/BUILD",
)

# Protobuf compiler binary
bind(
  name = "protoc",
  actual = "@com_google_protobuf//:protoc",
)

bind(
  name = "protocol_compiler",
  actual = "@com_google_protobuf//:protoc",
)

# Library needed to build protobuf codegen plugin.
bind(
  name = "protobuf_clib",
  actual = "@com_google_protobuf//:protoc_lib",
)

# Protobuf runtime
bind(
  name = "protobuf",
  actual = "@com_google_protobuf//:protobuf",
)

bind(
  name = "protobuf_java_lib",
  actual = "@com_google_protobuf//:protobuf_java",
)

bind(
  name = "protobuf_java_util",
  actual = "@com_google_protobuf//:protobuf_java_util",
)
