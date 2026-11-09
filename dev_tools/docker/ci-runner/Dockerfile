# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

FROM ghcr.io/edgcpp/build-base:latest

RUN mkdir /edg/tools

COPY setup-scripts/* /edg/setup-scripts/

# Execute root based setup scripts.
RUN /edg/setup-scripts/run-all.sh && rm -rf /edg/setup-scripts/**

# Copy and extract the current scripts-and-libs.tar.gz archive
COPY scripts-and-libs.tar.gz /edg/scripts-and-libs.tar.gz
RUN tar -xvf /edg/scripts-and-libs.tar.gz --directory=/edg/tools/

# Install docker tools.
COPY docker-tools/* /edg/docker-tools/

# Update PATH-like variables.
ENV PATH="/edg/docker-tools:/edg/tools/bin:${PATH}"
ENV PYTHONPATH="/edg/tools/pylibs"
