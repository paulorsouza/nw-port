# Contributing

## Before you start

1. Set up the project (README, "Getting started").
2. Read [docs/status.md](docs/status.md) and the spec of the area you want to work on in [docs/game/](docs/game/).
3. Read [docs/workflow.md](docs/workflow.md): areas, which files belong to which area, extension points, and how
   several people work without stepping on each other.

## Rules

- Nothing from the game goes into the repository: no extracted files, screenshots, sounds, meshes, text copied from
  the game's files, or Unreal assets imported from them. Code, tools, configuration that names game IDs, and docs
  describing the game in your own words are fine.
- No addresses of the game's executable in code or docs. Reverse engineering is asked on demand
  ([docs/tech/reverse-engineering.md](docs/tech/reverse-engineering.md)); answers stay on your machine.
- Numbers and behavior come from the game's data, with the source (file and ID) in a short comment or in the
  area's spec. No guessing: without a source, the item goes to the spec's open questions.
- Specs ([docs/game/](docs/game/)) state what is true now. History, measurements and discarded attempts go to
  [docs/notes/](docs/notes/).
- Docs and commit messages in English, short and direct.

## Changes

- One topic per pull request. Commit messages: `<area>: <what changed>` (e.g. `weapons(bow): ...`, `systems: ...`,
  `tools: ...`, `docs: ...`).
- Run `tools/linux/check.sh` (Linux) or `python tools/windows/doctor.py` and
  `python -m unittest discover -s tools/tests` (Windows) before opening the pull request; `--cpp` also compiles.
  GitHub Actions runs the same checks without the game (`check.sh --ci`, unit tests, pipeline dry runs) and the
  nw-core tests against Postgres on every push and pull request. The C++ is not built in CI (no Unreal there): build
  it locally.
- New behavior comes with a test item in [docs/tests/](docs/tests/) and the area spec updated.
- Visual changes: attach a screenshot from the editor. Do not add automated runs of the game.

## With Claude Code

Skills in `.claude/skills/` cover tool usage only (setup, extraction, Unreal, platform commands, running the game,
reverse engineering, asset backup). Game rules are in `docs/`, never in skills.
