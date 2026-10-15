# NW Exiles (nw-port)

A fan port of New World to Unreal Engine 5.8, for the community to keep playing and reworking the game after the
official servers close.

This is an unofficial project, not affiliated with Amazon Games ([NOTICE.md](NOTICE.md)). The repository holds code,
tools and documentation only. Every game asset is extracted on your machine from your own New World install;
nothing from the game is in the repository or downloaded from anywhere else.

## What works

- Windsward: the town, 2 km of region terrain, pre-generated scenery, 100,000 gatherables with tools, loot and
  respawn, music and ambient sound.
- Everfall: the town with NPCs, refining and crafting stations using the game's crafting screen, storage, a shop,
  levels from tutorials.
- Combat with the game's rules: sword and shield, hatchet, bow, rapier, ice gauntlet; creatures (wolf, boar) driven
  by the game's behavior trees.
- Dedicated Linux server, nw-core backend (Rust + Postgres: invite-only accounts, characters, inventory, storage),
  Docker images.
- Game screens: Esc menu, options, world map, compass, tooltips, XP and level, attributes, trade skills, mastery.

Details and known problems: [docs/status.md](docs/status.md).

## What you need

- New World installed through Steam, at the build in [tools/game.version](tools/game.version).
- Unreal Engine 5.8.3.
- Linux (tested on Ubuntu) or Windows (tools build and pass their tests; a full extraction on Windows is not
  accepted yet). 32 GB of RAM (64 GB recommended), a GPU with 8 GB of VRAM, about 150 GB free.

## Getting started

```bash
git clone https://github.com/paulorsouza/nw-port.git
cd nw-port
python3 tools/setup_status.py
```

`setup_status.py` reports what the machine has and which path applies. With [Claude Code](https://claude.com/claude-code),
ask for "setup": the `nw-setup` skill installs the tools, extracts, imports and opens the editor, and tells you
what only you can do (accounts, downloads). Without it, follow
[.claude/skills/nw-setup/](.claude/skills/nw-setup/SKILL.md) by hand: every step is a command. The first full build
takes 4 to 6 hours, almost all unattended.

## Repository

| Folder | Contents |
|---|---|
| `unreal/NWPort` | Unreal project (C++ in `Source/`, settings in `Config/`) |
| `tools/` | extraction and conversion pipeline (Python, shared by Linux and Windows); `tools/ue/` runs inside Unreal; `tools/linux/`, `tools/windows/` system scripts |
| `backend/` | nw-core: accounts, characters, inventory, storage, market |
| `deploy/` | Docker for the dedicated server and the backend |
| `patches/` | changes to nwbt (nw-buddy) and ooz, applied by the build scripts |
| `docs/` | documentation: [docs/README.md](docs/README.md) |
| `.claude/` | Claude Code skills (tool usage) and the reverse-engineering agent |

Not tracked: `extract/`, `bin/`, `vendor/`, `re-local/`, `local.env`, `unreal/NWPort/{Content,Data,Binaries,Intermediate,Saved}`.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md). Game rules and specs live in [docs/game/](docs/game/); open work in
[docs/backlog.md](docs/backlog.md).

## License

Code and documentation: [MIT](LICENSE). Third-party tools keep their own licenses ([NOTICE.md](NOTICE.md)).
