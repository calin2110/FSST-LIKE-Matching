FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    gdb \
    rsync \
    tar \
    pkg-config \
    zlib1g-dev \
    libzstd-dev \
    libcurl4-openssl-dev \
    llvm-16-dev \
    clang \
    lldb \
    libvectorscan-dev \
    && rm -rf /var/lib/apt/lists/*

# Set environment variables so CMake automatically uses the installed Clang
ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

WORKDIR /app