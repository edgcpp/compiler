# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

FROM fedora:43

RUN mkdir /edg && \
    mkdir /edg/setup-scripts && \
    mkdir /edg/docker-tools && \
    mkdir /edg/user-script/ && \
    mkdir /edg/user-script/pip-packages/
COPY setup-scripts/* /edg/setup-scripts/

# Execute root based setup scripts.
RUN /edg/setup-scripts/run-all.sh && rm -rf /edg/setup-scripts/**

# Override resolution of sh to dash (to ensure POSIX compatibility on sh-based
# shebang lines).
RUN ln -sf /usr/bin/dash /usr/bin/sh

# Install native tools.
COPY --from=ghcr.io/edgcpp/native-tools:latest \
  /edg/native-tools \
  /edg/native-tools

# Install softfloat tools.
COPY --from=ghcr.io/edgcpp/softfloat:latest \
  /edg/softfloat \
  /edg/softfloat

# Update PATH-like variables.
ENV PYTHONUSERBASE="/edg/user-script/pip-packages/"
ENV PATH="/edg/native-tools/bin:${PYTHONUSERBASE}bin:${PATH}"

# Tell CMake where to find softfloat.
ENV EDG_SOFTFLOAT_INCLUDE_PATH="/edg/softfloat/include"
ENV EDG_SOFTFLOAT_LIB_BASE_PATH="/edg/softfloat/lib"

# Set some CPFE configuration variables for Docker.
ENV EDG_RUNTIME_LIB="edgrt"
