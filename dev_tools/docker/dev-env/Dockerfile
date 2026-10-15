# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

FROM ghcr.io/edgcpp/build-base:latest

RUN mkdir /edg/workspace && \
    mkdir /edg/opt-software && \
    mkdir /edg/usr-local-software

COPY extra-software/opt/* /edg/opt-software/
COPY extra-software/usr-local/* /edg/usr-local-software/
COPY setup-scripts/* /edg/setup-scripts/

# Execute root based setup scripts.
RUN /edg/setup-scripts/run-all.sh && rm -rf /edg/setup-scripts/**

# Install docker tools.
COPY docker-tools/* /edg/docker-tools/

# Update PATH-like variables.
ENV PATH="/edg/docker-tools:${PATH}"
