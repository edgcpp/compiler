# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Run acknowledg-forge against GitHub (configuration: see README.md).'''

from edgacknowledg.forge import service
from edgacknowledg.forge.config import ForgeCredentials, ForgeServiceConfig

from acknowledg_forge_github import (
  DEFAULT_API_URL,
  GitHubClient,
  GitHubCommenter,
  GitHubForge,
  GitHubWebhooks,
)


def build(config: ForgeServiceConfig,
           credentials: ForgeCredentials) -> service.ForgeComponents:
  client = GitHubClient(
    api_url = config.api_url or DEFAULT_API_URL,
    token = credentials.forge_token
  )
  return service.ForgeComponents(
    forge = GitHubForge(client, workflow = config.ci_workflow),
    webhooks = (
      GitHubWebhooks(credentials.webhook_secret) if credentials.webhook_secret
      else None
    ),
    commenter = GitHubCommenter(client) if credentials.forge_token else None,
  )


def main() -> None:
  service.main(build)


if __name__ == '__main__':
  main()
