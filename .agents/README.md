# Portable agent skills

Project skills live here in the open
[Agent Skills](https://agentskills.io/) format. Each subdirectory contains a
`SKILL.md` with YAML frontmatter (`name`, `description`) and instructions.

```
.agents/skills/<skill-name>/SKILL.md
```

Do not add parallel copies under tool-specific paths (`.cursor/skills/`,
`.claude/skills/`, etc.); keep a single tree under `.agents/skills/`.

Always-on project guidance for all agents: [`AGENTS.md`](../AGENTS.md) at the
repository root. Changelog encoding: skill `edg-changes-example`.
