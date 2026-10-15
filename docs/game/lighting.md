# Lighting

Goal: light equal to the game (day cycle, sky, sun, exposure, local lights, interiors) at an acceptable cost on an
8 GB graphics card. Each item chooses between converting the game's data and using the equivalent Unreal feature
(Lumen, MegaLights, physical units, Local Exposure, Volumetric Fog), measured against a screenshot of the original
game.

Maps: `L_QuedaEterna` first (interiors: church, tavern), then `L_SouthWindsward`. History, including an undone
attempt that must not be repeated: [notes/world.md](../notes/world.md).

## Rules

- No invented constants. A number without a source in the data becomes a question for reverse engineering (ask the
  `nw-re` agent) before it becomes code.
- Each choice (convert or Unreal feature) is measured at one point (the Everfall church) against a screenshot of the
  original game, with `stat gpu` and VRAM, before it goes to the whole city.
- Lights and volumes enter the map by script in `tools/ue/`, in a step `iluminacao-NN` (`tools/etapas/iluminacao.*`).
  Never by hand in the editor.
- The maps are shared with the systems area (Everfall) and with gathering and mounts (Windsward). Lighting actors
  go only in their own folder (`Iluminacao/`); do not touch the others.
- The lighting area owns `tools/etapas/iluminacao.*`, `tools/ue/city_exposure.py` (it came from the systems area),
  the light scripts in `tools/ue/` and the `sliceLight` change in the nwbt patch ([workflow.md](../workflow.md)).
- Show the plan to the maintainer before changing project configuration (`DefaultEngine.ini` is shared).
- Start with the survey: send the questions below to reverse engineering, list the fixed points where the
  maintainer takes screenshots in the original game (Everfall square, church and tavern, a street at night), and
  show the plan with the choice for each item, the expected cost and how it will be measured, before changing code
  or the map.
- Tests: no lighting test list exists yet. Create one under `docs/tests/` with the first item
  ([tests/README.md](../tests/README.md)).

## Current setup

- Per map: sun (`DirectionalLight`, 8 lux, fixed), sky light (real-time capture), atmosphere and fog
  (`tools/ue/import_city.py`). Lumen on (GI and reflections), virtual shadow maps, Nanite, distance fields
  (`unreal/NWPort/Config/DefaultEngine.ini`).
- Auto exposure is off in the project (`r.DefaultFeature.AutoExposure=False`): Unreal locks luminance at 1
  (`SceneView.cpp`). `L_QuedaEterna` has a volume `ExposicaoDoJogo` (`tools/ue/city_exposure.py`, step
  `sistemas-04`) that lets only dark areas brighten up to 10x (range 0.01–0.1, bias −2.32). The street stays as
  before. To be reviewed by this area.
- No light from the game: the conversion (`tools/linux/convert_city.sh`) only brings meshes.

### Known issue: Lumen surface cache

In Everfall the editor warns that the Lumen surface cache atlas is oversubscribed by 3–10% and suggests raising
`r.LumenScene.SurfaceCache.AtlasSize`. The city has ~5,000 mesh components and the default cache does not fit:
parts of the scene lose updated indirect light (patches, flickering GI). The message appears only in
editor/development builds, but the problem exists in the final game. Options to measure:

- raise the atlas size (costs VRAM);
- fewer cards per mesh (`r.LumenScene.SurfaceCache.CardMinSize`; small meshes out of Lumen);
- merge small props;
- Lumen with hardware ray tracing.

## Game data already surveyed

### Day cycle

`libs/timeofday/zones/everfall_a.xml` (Everfall zone; extracted to `extract/_raw/names/libs/timeofday/`). Curves per
hour of day (key `fraction:value:flags`). Daytime values:

| Parameter | Value |
|---|---|
| "Sun intensity" | 100,000 lux |
| "Sun color multiplier" | 10 |
| "EV Min" | 5 |
| "EV Max" | 17 |
| "EV Auto compensation" | 1.5 |
| "Min exposure" | 1 |
| "Max exposure" | 10 |

The file also has fog, sky, bloom and film curve ("Film curve").

### Local lights

Component `LightComponent` → `LightConfiguration` in the slices (struct in
`vendor/nw-buddy/tools/nwbt/rtti/nwt/generated.go`). Fields:

- `LightType` (0 point, 1 area, 2 projector, 3 probe)
- `Color`, `DiffuseMultiplier`, `SpecMultiplier`
- `PointMaxDistance`, `PointAttenuationBulbSize`
- `AreaWidth/Height`, `AreaMaxDistance`, `AreaFOV`
- `ProjectorDistance/FOV/Texture`
- `CastShadowsSpec` (0xFFFFFFFF = never)
- `ViewDistanceMultiplier`, `ViewDistanceCap`
- `AnimIndex/AnimSpeed/AnimPhase` (flicker)
- `AffectsThisAreaOnly`, `IndoorOnly`, `IgnoreVisAreas`, `TodInfluence`, `VolumetricFog(Only)`, `VoxelGIMode`,
  `Ambient`

### Everfall city

- 744 visible lights: 466 point, 73 area, 205 projector; 5 with shadows.
- Church (`struc_church_t5`): 66 (36/16/14). Candles with `DiffuseMultiplier` 0.15–0.3 and range 2 m, area 0.1.
- Tavern (`ef_settlement_inn`): 49; fireplace (projector up to 20).
- Street lamps: `settler_footprinta_northwindsward_setdress_msq2_lights`.
- Light data file: `extract/cities/luzes/settlement_northwindsward_complete.lights.json`.

### nwbt

`models slices --lights` exports the lights (KHR_lights_punctual). The port's `sliceLight` in
`commands/models/collect_capitals.go` writes the type, the correct range and all the parameters above into the
extras of node `nwlight_N`. It is part of `patches/nwbt.patch`.

## Questions for reverse engineering

1. Unit and formula of a local light: how `DiffuseMultiplier` and the color become radiance in the game's shader
   (HDR scale, relation to the 100,000 lux sun and to exposure).
2. Attenuation: the curve of the radius (`PointMaxDistance`) and of `AttenuationBulbSize`; area light (rectangle,
   `AreaFOV`).
3. View distance and light culling: formula with `ViewDistanceMultiplier` and the equivalent cvar
   (`e_ViewDistRatioLights`?), and whether there is a fade.
4. Interiors: `AffectsThisAreaOnly`/`IndoorOnly` and vis areas. Is sun/sky light cut inside buildings?
5. The game's GI: is there dynamic GI (`VoxelGIMode`, SVOGI) or probes (`LightType` 3, cubemaps) in interiors?
6. Exposure: how "EV Min/Max", "EV Auto compensation" and "Min/Max exposure" of the day cycle enter the
   calculation.
7. Flicker: the `AnimIndex` table (curve per index) and `AnimSpeed`/`AnimPhase`.

## Unreal vs conversion

To be decided in the survey.

| Topic | Convert from the game | Unreal feature |
|---|---|---|
| Sun, sky, day cycle | curves from `everfall_a.xml` | Sky Atmosphere, real-time Sky Light, physical units (lux/EV100) |
| Exposure | the game's EV Min/Max | auto exposure in EV100, Local Exposure |
| GI and interiors | the game's probes/vis areas | Lumen (software or HW ray tracing), emissive on candles and windows |
| Many local lights | position, color, range from the game | MegaLights (5.5+), lights without shadows, distance culling with fade |
| Shadows | `CastShadowsSpec` | Virtual Shadow Maps, contact shadows |
| Flicker | the `AnimIndex` table | Light Function or an animation component |
| Fog and volume | fog curves of the day cycle | Exponential Height Fog, Volumetric Fog |

Criterion: measure each choice (original game screenshot vs port at the same point, `stat gpu`, VRAM) before
applying it to the whole city.

## Suggested order

1. Survey and reverse engineering (questions above); screenshots of the original game at fixed points (square,
   church, tavern, street at night).
2. Sun, sky, day cycle and exposure in physical units, taken from `everfall_a.xml`.
3. Local lights with the formula from reverse engineering, in one building only (the church), until it matches the
   screenshot.
4. Interiors (GI, cutting outside light) and flicker.
5. Performance (MegaLights, culling, scalability), measured.
6. The whole city, then Windsward.
