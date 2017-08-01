FROM debian:9.1

RUN apt-get update && \
    apt-get install -y --no-install-recommends pkg-config zip unzip zlib1g-dev wget gcc g++ openjdk-8-jdk-headless && \
    apt-get clean

RUN wget https://github.com/bazelbuild/bazel/releases/download/0.5.3/bazel-0.5.3-without-jdk-installer-linux-x86_64.sh && \
    chmod +x bazel-0.5.3-without-jdk-installer-linux-x86_64.sh && \
    ./bazel-0.5.3-without-jdk-installer-linux-x86_64.sh && \
    rm bazel-0.5.3-without-jdk-installer-linux-x86_64.sh

COPY dumb-init /sbin/
RUN groupadd -g 12345 user && useradd -m -u 12345 -g 12345 user
USER user
WORKDIR /home/user
ENTRYPOINT ["/sbin/dumb-init", "--"]
