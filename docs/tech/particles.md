# Particles

How the game builds its particle effects, and how the port converts them to Niagara. The rule: game effects become
Niagara systems, built by script from the game's particle libraries. History of the decision and the discarded
option: [../notes/combat.md](../notes/combat.md).

Reference case: the Ice Storm of the Ice Gauntlet (`IceMagic_Spell_IceStorm`). Study files in `extract/fx_study/`.

## How the game builds an effect

- The spell spawns the slice `items/weapons/spell_aoe_blizzard`, which has two `ParticleComponent`s:
  `wfxo_IceGaunt_Tempest.Blizzard_AOE` (library `libs/particles/wfxo_icegaunt_tempest.xml`) and
  `pex_foe_vfx.aoe_vfx_base_0` (`libs/particles/pex_foe_vfx.xml`, generic enemy-area ring). It also has a timeline
  (`libs/timeline/weapons/icegauntlet_blizzard_01.timeline`, PostFX layer).
- The library is CryEngine XML (`ParticleVersion 33`): a root emitter with children, each with `<Params>` of ~25
  attributes. Curves over particle age (`ParticleAge=(t=..,v=..)`), randomness (`Random=`), texture atlas
  (`TilesX/TilesY/VariantCount`), facing mode (`CameraX`, `Free`, `Decal`, `Shape`), CPU or GPU.

`Blizzard_AOE` (5 active emitters):

| Emitter | Type | What it is | Texture / material |
|---|---|---|---|
| root | mesh (`sphere_edge.cgf`, 6 m) | spinning dome, 2 at a time, lifetime 1 s, fades in and out over 2 s | `icegauntlet_blizzard_aoe_mesh_2-optimize` (shader `Meshparticle`, noise `noise_alpha_01` + emissive `noise_02`) |
| `2_Decal_Spin` | decal | snow circle spinning 360°/s, 4.5 m | `icegauntlet_decal_02` (`snow_r_circle001` + snow normal) |
| `2_Decal_Static_Edge` | decal | ice crack, 5.25 m, lifetime 5.7 s | `impact_frost_03` (`groundcrack_04` + `ice_001`) |
| `2_frostdecal` | decal | ice on the ground, 4.8 m, lifetime 5.7 s | `impact_frost_2_less` (`ice_001`) |
| `3_Sphere_Edge_Flurry_gpu` | GPU sprites | flakes on the 9 m ring, 50 every 0.2 s, rise and spin | `Embers_R_Single002_2x2` (2×2 atlas) |

What the effect needs: 1 mesh, 3 decals, 1 sprite emitter, 6 textures, 4 materials with 3 shaders (`Meshparticle`,
`Particles` as decal, `Particles` as sprite).

nwbt converts meshes and textures (`.dds`/`.tif` → png). It does not read `libs/particles/*.xml` (plain XML, read
directly by our script) nor the effect shaders.

## Decision: Niagara

Game effects are rebuilt as Niagara systems. The editor C++ (`UNWNiagaraTools`, `NWNiagaraTools.cpp`), called from
Python in a commandlet, works without opening the editor:

- creates a system from an engine template emitter (`Fountain`);
- lists the modules, reads and writes constant parameters (`RapidIterationParameters`: lifetime, size, color, cone
  velocity, gravity, drag, spawn rate, shape radius);
- replaces curve keys (`UNiagaraDataInterfaceCurve` of the input nodes, through reflection);
- replaces the sprite material and atlas;
- compiles and saves.

Available in UE 5.8: sprite, mesh, decal and light renderers, and the modules `ScaleSpriteSize`, `ScaleColor`,
`ScaleMeshSize`. Exported from NiagaraEditor: `AddEmitterToSystem`, `AddScriptModuleToStack`, `SetModuleIsEnabled`,
`GetOrCreateStackFunctionInputOverridePin`, `SetDataInterfaceValueForFunctionInput`. Not exported:
`GetOrderedModuleNodes` and the class `UNiagaraNodeInput` (worked around by walking the graph nodes and by
reflection). The probe of this API is `UNWNiagaraTools::ProbeNiagara`.

The same format covers fire, smoke and the effects of other weapons. Ownership: the shared code (`NWFxActor`, the
Niagara tools) belongs to the weapons area; using effects in the scenery belongs to the lighting area
([../game/lighting.md](../game/lighting.md)). `ANWFxActor` (spheres with material `M_NW_FX`, color + intensity) is
the older placeholder effect: no sprites, no decals, no atlas.

## Pipeline

1. Extract:

   ```bash
   python3 tools/make_particles.py --extract <library.xml> <Effect> [...]
   ```

   XML → `extract/fx/<Lib>.<Effect>.json` (emitters, curves, texture/mesh/material list) and, through
   `tools/linux/heavy.sh`, materials (`.mtl`), textures (png) and meshes (glb) into `extract/fx/src` and `extract/fx/glb`.

2. Build, with the editor closed:

   ```bash
   UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_fx.py extract/fx/<json> [...]"
   ```

   It imports textures and meshes, creates the masters (`M_NW_P_Sprite`, `_SpriteAdd`, `_Mesh`, `_MeshLit`,
   `_Decal`) and one instance per game material, translates each emitter into a recipe (`.recipe.json`) and calls
   `UNWNiagaraTools::BuildEffect`. Report in `.build.txt` (lists the constants not found). Do not run it inside an
   open editor: with dozens of systems the editor hangs.

3. `tools/ue/fx_notifies.py` places the hand and body effects on the montages. Run it again after reimporting the
   player set.

Output assets: `/Game/NW/FX/NS_<Library>_<Effect>`.

## What `BuildEffect` must handle

- Module constants of the emitters are compiled into the system scripts. They are written to the emitter scripts and
  also to the system spawn/update scripts; otherwise the effect falls back to the template values.
- `Loop Duration` of the system and of the emitters = 1000 s (the `Fountain` template dies in 5 s).
- An existing package is loaded in full and the old system renamed before saving over it. Saving over a partially
  loaded package crashes the commandlet.
- Niagara decal: the size is the full size (2× the game radius) and the orientation is (0; 0.7071; 0; 0.7071) to
  project downward. Characters have `SetReceivesDecals(false)`.

## Not supported yet

- Light (`LightSource`), ribbons (`Connection`), mesh size curve, color and size curves (alpha only), decal alpha
  curve.
- `EmitterStrength`, `MotionBlur`, `SoftParticle`, `CameraNonFacingFade` and the PostFX timeline.

## Pitfalls

- Do not open the editor while the original New World is running: together they exceeded the 8 GB of the graphics
  card and hung the driver (a reboot was needed).
