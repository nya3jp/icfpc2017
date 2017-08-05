ICFP Programming Contest 2017
=============================

Prerequisites
-------------

Bazel is required to build C++ binaries. See this page for instructions:

https://docs.bazel.build/versions/master/install-ubuntu.html

Or if you are impatient, type following commands:

```
$ wget https://github.com/bazelbuild/bazel/releases/download/0.5.3/bazel-0.5.3-without-jdk-installer-linux-x86_64.sh
$ chmod +x bazel-0.5.3-without-jdk-installer-linux-x86_64.sh
$ ./bazel-0.5.3-without-jdk-installer-linux-x86_64.sh --prefix=$HOME/local/bazel
```

Examples
--------

Example codes are in examples/ directory.

How to build:

```
$ bazel build //examples:minimal_main
$ bazel-bin/examples/minimal_main
```

How to test:

```
$ bazel test //examples:minimal_lib_test
```
