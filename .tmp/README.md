# Mono Repository TMP

This directory is not intended for use as general temporary storage.  Its
purpose is to faciliate bridging temporary files from the host to docker (e.g.,
a file can be generated on the host, stored here, and then referenced from a
command executed in docker).

This allows for the host to pass non-trivial empherial information to Docker
for further processing (that if packaged entirely as command line arguments
would exceed the maximum command line argument limit -- primarily a problem
on Windows).

## Notes for Agents

This section is an instruction for AI coding agents that come across this
file; it is not required reading for human users.

Agents may use this directory for temporary files and directories that must be
visible both on the host and inside the docker container (the project root is
mounted at `/edg/workspace`, so `.tmp/agent-foo` on the host is
`/edg/workspace/.tmp/agent-foo` in the container).  Everything an agent creates
here must be named `agent-<purpose>` so it can be told apart from (and cleaned
up separately from) anything else in this directory.  Agents remove these
temporary files and directories in a batch step as the last step before
reporting results to the user.

See the `edg-agent-temp-files` skill (`.agents/skills/edg-agent-temp-files/`)
for the full rules.
