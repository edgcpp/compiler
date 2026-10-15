# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Run acknowledg-forge against GitLab (configuration: see README.md).'''

from edgacknowledg.forge import service
from edgacknowledg.forge.config import ForgeServiceConfig

from acknowledg_forge_gitlab import (
  DEFAULT_API_URL,
  GitLabClient,
  GitLabCommenter,
  GitLabForge,
  GitLabWebhooks,
)


def build(config: ForgeServiceConfig) -> service.ForgeComponents:
  client = GitLabClient(
    api_url = config.api_url or DEFAULT_API_URL,
    token = config.forge_token
  )
  return service.ForgeComponents(
    forge = GitLabForge(
      client,
      pipeline = config.ci_workflow,
      artifact_jobs = (config.test_artifact, config.bench_artifact)
    ),
    webhooks = (
      GitLabWebhooks(config.webhook_secret) if config.webhook_secret
      else None
    ),
    commenter = GitLabCommenter(client) if config.forge_token else None,
  )


def main() -> None:
  service.main(build)


if __name__ == '__main__':
  main()
