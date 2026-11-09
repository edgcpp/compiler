# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

FROM centos:7

RUN mkdir /edg && \
    mkdir /edg/workspace && \
    mkdir /edg/tools && \
    mkdir /edg/tests && \
    mkdir /edg/setup-scripts && \
    mkdir /edg/docker-tools && \
    mkdir /edg/cmake/ && \
    mkdir /edg/cmake/setup/
COPY setup-scripts/* /edg/setup-scripts/
# Download a more recent CMake, if this updated, change extract-cmake.sh
ADD https://github.com/Kitware/CMake/releases/download/v3.19.8/cmake-3.19.8-Linux-x86_64.tar.gz \
    /edg/cmake/setup/cmake.tar.gz

# Swap to mirrors that still work.
RUN sed -i s/mirror.centos.org/vault.centos.org/g /etc/yum.repos.d/*.repo && \
    sed -i s/^#.*baseurl=http/baseurl=http/g /etc/yum.repos.d/*.repo && \
    sed -i s/^mirrorlist=http/#mirrorlist=http/g /etc/yum.repos.d/*.repo

# Execute root based setup scripts.
RUN /edg/setup-scripts/run-all.sh

# Install docker tools.
COPY docker-tools/* /edg/docker-tools/

# Install native tools.
COPY --from=ghcr.io/edgcpp/legacy-native-tools:latest \
  /edg/native-tools \
  /edg/native-tools

# Set the path.
ENV PATH="/edg/docker-tools:/edg/native-tools/bin:/edg/tools/bin:/edg/cmake/install/bin:${PATH}"

# Set some CPFE configuration variables for Docker.
ENV EDG_RUNTIME_LIB="edgrt"
# This isn't working, fix it later.
#ENV EDG_USE_GPP_LIB=1
