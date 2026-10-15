"""Auto exposure on a city map (area 4): unbound post-process volume, on this map only.

    UnrealEditor-Cmd ... -run=pythonscript -script="tools/ue/city_exposure.py L_QuedaEterna"

The project turns auto exposure off (DefaultEngine.ini, r.DefaultFeature.AutoExposure=False): without a volume, Unreal
locks the average luminance at 1 (SceneView.cpp) and the eye does not adapt when entering a building. The game adapts
(Everfall day cycle, libs/timeofday/zones/everfall_a.xml, daytime): "Min exposure" 1 and "Max exposure" 10, that is,
exposure goes from 1x (the street's) to 10x in the dark.

The port's street measures well below 1 (8 lux sun; in the game, 100,000): a 0.1–1 range brightened the street too. So
the range sits below the street (0.01–0.1), the street always at the ceiling, and the bias offsets the ceiling change
(1 -> 0.1): the street looks as with the old fixed exposure and only the dark brightens, up to 10x. Speeds: engine's.
"""
import math
import sys

import unreal

MAP = sys.argv[1] if len(sys.argv) > 1 else "L_QuedaEterna"
LABEL = "ExposicaoDoJogo"
MAX_BRIGHTNESS = 0.1                    # below the street luminance: the street stays at the ceiling
MIN_BRIGHTNESS = MAX_BRIGHTNESS / 10.0  # everfall_a.xml "Max exposure" 10 / "Min exposure" 1
BIAS = 1.0 + math.log2(MAX_BRIGHTNESS)  # default bias (r.DefaultFeature.AutoExposure.Bias 1) plus the ceiling change 1 -> 0.1

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(f"/Game/NW/Maps/{MAP}")

ppv = next((a for a in eas.get_all_level_actors() if isinstance(a, unreal.PostProcessVolume) and a.get_actor_label() == LABEL), None)
if ppv is None:
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    ppv.set_actor_label(LABEL)
ppv.set_editor_property("unbound", True)
s = ppv.settings
s.set_editor_property("override_auto_exposure_method", True)
s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM)
s.set_editor_property("override_auto_exposure_min_brightness", True)
s.set_editor_property("auto_exposure_min_brightness", MIN_BRIGHTNESS)
s.set_editor_property("override_auto_exposure_max_brightness", True)
s.set_editor_property("auto_exposure_max_brightness", MAX_BRIGHTNESS)
s.set_editor_property("override_auto_exposure_bias", True)
s.set_editor_property("auto_exposure_bias", BIAS)
ppv.settings = s
les.save_current_level()
unreal.log(f"EXPOSURE {MAP}: histogram {MIN_BRIGHTNESS}..{MAX_BRIGHTNESS} bias {BIAS:.2f}")
