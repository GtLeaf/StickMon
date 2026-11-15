# Project Design Skills

Installed from upstream using the documented manual-copy method on 2026-11-15.

| Skill | Entry point | Usage |
| --- | --- | --- |
| Impeccable 4.9.1 | [SKILL.md](impeccable/SKILL.md) | `$impeccable typeset <target>` for typography; other commands are bundled |
| Game UI / UX | [SKILL.md](game-ui-ux/SKILL.md) | `$game-ui-ux <request>` for in-game interface work |

Impeccable's typography playbook is [typeset.md](impeccable/reference/typeset.md).
Its launcher is `impeccable/scripts/impeccable`; the verified engine reports
`impeccable-engine 0.1.24`. The launcher uses its standard user cache.
Game UI / UX is a single-file skill with no runtime dependencies.

Repository URLs, pinned commits and licenses are recorded in
[installed-sources.json](installed-sources.json). Each skill includes its upstream
license. These are project-local skills; refresh the coding tool's skill discovery
after installation when needed.

This manual installation includes the skill bundles, without installing editor
hooks or running project initialization. Read the skill entry point when invoking it.
