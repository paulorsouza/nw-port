"""Exposição automática num mapa de cidade (esteira 4): volume de pós-processo sem limites, só neste mapa.

    UnrealEditor-Cmd ... -run=pythonscript -script="tools/ue/city_exposure.py L_QuedaEterna"

O projeto desliga a exposição automática (DefaultEngine.ini, r.DefaultFeature.AutoExposure=False): sem volume, a Unreal
trava a luminância média em 1 (SceneView.cpp) e o olho não se adapta ao entrar num prédio. O jogo adapta (ciclo de dia
de Queda Eterna, libs/timeofday/zones/everfall_a.xml, de dia): "Min exposure" 1 e "Max exposure" 10, ou seja, a
exposição vai de 1x (a da rua) a 10x no escuro.

A rua do port mede bem menos que 1 (sol de 8 lux; no jogo, 100.000): uma faixa 0,1–1 clareava a rua também. Por isso a
faixa fica abaixo da rua (0,01–0,1), a rua sempre no teto, e o viés compensa a troca do teto (1 -> 0,1): a rua sai igual à
exposição fixa de antes e só o escuro clareia, até 10x. Velocidades ficam as do motor.
"""
import math
import sys

import unreal

MAP = sys.argv[1] if len(sys.argv) > 1 else "L_QuedaEterna"
LABEL = "ExposicaoDoJogo"
MAX_BRIGHTNESS = 0.1                    # abaixo da luminância da rua: a rua fica no teto
MIN_BRIGHTNESS = MAX_BRIGHTNESS / 10.0  # everfall_a.xml "Max exposure" 10 / "Min exposure" 1
BIAS = 1.0 + math.log2(MAX_BRIGHTNESS)  # viés padrão (r.DefaultFeature.AutoExposure.Bias 1) mais a troca do teto 1 -> 0,1

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
unreal.log(f"EXPOSICAO {MAP}: histograma {MIN_BRIGHTNESS}..{MAX_BRIGHTNESS} viés {BIAS:.2f}")
