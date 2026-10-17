# Documentation

## Start here

| Doc | For |
|---|---|
| [setup.md](setup.md) | setting up a machine (the full procedure is the `nw-setup` skill) |
| [status.md](status.md) | what works, known problems, pitfalls, repository map |
| [workflow.md](workflow.md) | areas, file owners, extension points, working in parallel, reverse engineering on demand |
| [backlog.md](backlog.md) | open work by area |

## Game (specs)

What the port implements and the rules it follows. One file per topic; current state only.

| Doc | Topic |
|---|---|
| [game/combat.md](game/combat.md) | combat rules for all weapons |
| [game/weapons/](game/weapons/README.md) | weapons: [bow](game/weapons/bow.md), [rapier](game/weapons/rapier.md), [ice gauntlet](game/weapons/ice-gauntlet.md) |
| [game/items.md](game/items.md) | items, armor, jewelry, perks, gear score |
| [game/crafting.md](game/crafting.md) | trade skills, stations, refining, crafting |
| [game/gathering.md](game/gathering.md) | Mining, Logging, Harvesting, Skinning, Fishing |
| [game/city.md](game/city.md) | NPCs, storage, shops, Trading Post, housing, character level and XP |
| [game/economy.md](game/economy.md) | territory governance and economy |
| [game/mounts.md](game/mounts.md) | mounts |
| [game/creatures-ai.md](game/creatures-ai.md) | creature AI from the game's behavior trees |
| [game/lighting.md](game/lighting.md) | lighting |
| [game/dungeon.md](game/dungeon.md) | dungeon proposal |
| [game/world/windsward.md](game/world/windsward.md) | the Windsward region |
| [game/world/everfall.md](game/world/everfall.md) | the Everfall town |
| [game/ui/](game/ui/README.md) | game screens: [original game](game/ui/original.md), [inventory](game/ui/inventory.md), [refining](game/ui/refining.md) |

## Technical

| Doc | Topic |
|---|---|
| [tech/architecture.md](tech/architecture.md) | client, dedicated server and nw-core; rules every area follows |
| [tech/server.md](tech/server.md) | dedicated server in the cloud: phases, security, packaging |
| [tech/asset-pipeline.md](tech/asset-pipeline.md) | how each asset type leaves the game and reaches Unreal |
| [tech/weapon-porting.md](tech/weapon-porting.md) | step by step to port or refine a weapon |
| [tech/particles.md](tech/particles.md) | game effects in Niagara |
| [tech/tools.md](tech/tools.md) | third-party tools and every script |
| [tech/reverse-engineering.md](tech/reverse-engineering.md) | Ghidra and the `nw-re` agent (method only) |
| [tech/backend-testing.md](tech/backend-testing.md) | testing with nw-core from the editor |
| [tech/admin.md](tech/admin.md) | admin page: players, bots, coins, summons and creature AI from the browser |
| [tech/network-test.md](tech/network-test.md) | network test with remote players |
| [tech/asset-sync.md](tech/asset-sync.md) | backing up the imported project to your other machines |
| [tech/windows.md](tech/windows.md) | native Windows: state, tools, migrating a checkout |

## Tests and notes

- [tests/](tests/README.md): test checklists per area.
- [notes/](notes/): dated history per area (decisions, measurements, discarded attempts). Specs never depend on notes.

Other READMEs: [backend/README.md](../backend/README.md) (nw-core), [patches/README.md](../patches/README.md) (nwbt and
ooz patches), [tools/steps/README.md](../tools/steps/README.md) (extra pipeline steps per area).
