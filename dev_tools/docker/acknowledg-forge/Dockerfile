# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# acknowledg-forge services (GitHub and GitLab), with a frozen copy of the EDG
# tooling they run and their locked Python dependencies. One image serves
# every container of a deployment (the service, the builder and tools
# runners, and the publisher worker's supervisor); see
# dev_tools/services/acknowledg-forge/compose.yaml and README.md.
#
# The image is public: it carries no credentials or deployment configuration
# (those come from the host at run time). It has no default command either;
# compose.yaml picks each container's.

FROM fedora:43

RUN mkdir -p /edg/setup-scripts /edg/tools /edg/acknowledg-forge
COPY setup-scripts/* /edg/setup-scripts/

# Execute root based setup scripts.
RUN /edg/setup-scripts/run-all.sh && rm -rf /edg/setup-scripts

# The frozen EDG tooling (dev_tools/bin and dev_tools/pylibs) the service runs
# (edg-rematerialize-test-run, ifc-dump); see prebuild.py.
COPY scripts-and-libs.tar.gz /edg/scripts-and-libs.tar.gz
RUN tar -xzf /edg/scripts-and-libs.tar.gz --directory=/edg/tools && \
    rm /edg/scripts-and-libs.tar.gz

# One virtual environment per service: the locked (hash-checked)
# dependencies, then the service wheel itself.
COPY services/ /edg/acknowledg-forge/dist/
ENV PIP_DISABLE_PIP_VERSION_CHECK=1
RUN for forge in github gitlab; do \
      venv="/edg/acknowledg-forge/${forge}" && \
      python3 -m venv "${venv}" && \
      "${venv}/bin/pip" install --no-cache-dir --require-hashes \
        -r "/edg/acknowledg-forge/dist/${forge}/requirements.txt" && \
      "${venv}/bin/pip" install --no-cache-dir --no-deps \
        /edg/acknowledg-forge/dist/${forge}/*.whl && \
      ln -s "${venv}/bin/acknowledg-forge-${forge}" /usr/local/bin/ ; \
    done && \
    rm -rf /edg/acknowledg-forge/dist

RUN useradd --create-home --uid 1000 acknowledg && \
    mkdir -p /var/lib/acknowledg-forge && \
    chown acknowledg:acknowledg /var/lib/acknowledg-forge

ENV ACKNOWLEDG_FORGE_TOOLS_DIR=/edg/tools
ENV ACKNOWLEDG_FORGE_HOME=/var/lib/acknowledg-forge

USER acknowledg
WORKDIR /var/lib/acknowledg-forge
