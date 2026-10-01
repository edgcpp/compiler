# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

FROM fedora:44

RUN mkdir /edg && \
    mkdir /edg/workspace && \
    mkdir /edg/tools && \
    mkdir /edg/tests && \
    mkdir /edg/setup-scripts && \
    mkdir /edg/docker-tools && \
    mkdir /edg/user-script/ && \
    mkdir /edg/user-script/pip-packages/
COPY setup-scripts/* /edg/setup-scripts/

# Copy in doc/requirements.txt (see prebuild.py) for the pip packages.
COPY requirements.txt /edg/setup-scripts/requirements.txt

# Execute root based setup scripts.
RUN /edg/setup-scripts/run-all.sh

# Install docker tools.
COPY docker-tools/* /edg/docker-tools/

# Set the path.
ENV PYTHONUSERBASE="/edg/user-script/pip-packages/"
ENV PATH="/edg/docker-tools:/edg/tools/bin:${PYTHONUSERBASE}bin:${PATH}"
