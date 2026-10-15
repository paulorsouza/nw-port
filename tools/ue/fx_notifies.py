"""Efeitos de mão e de corpo da manopla de gelo nas montagens AM_Ig_* (procclips ParticleEffect do .adb, convertidos por
tools/ue/import_fx.py). Notify state "Timed Niagara Effect" no osso do procclip: todo cliente vê no tempo da animação.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script=tools/ue/fx_notifies.py

Rodar de novo depois de reimportar o conjunto (o import refaz as montagens). Trilha "FX" é refeita a cada rodada.
"""
import unreal

ANIMS = "/Game/NW/sword_shield/Anims"
FX = "/Game/NW/FX/NS_{0}.NS_{0}"
# montagem: [(efeito, osso, início s, fim s ou None = fim da montagem)] — fragmento e tempo do procclip
# (player_magicgauntlet_ice.adb); "Xform" do jogo = raiz do personagem (sem osso).
MAP = {
    "AM_Ig_Primary": [("wFX_IceGaunt_Base_Primary_Ready", "right_hand_attach", 0.0, 0.2),
                      ("wFX_IceGaunt_Base_Primary_Muzzle", "right_hand_attach", 0.168, 0.5)],
    "AM_Ig_Heavy": [("wFX_IceGaunt_Base_Heavy_Muzzle", "right_hand_attach", 0.168, 0.5)],
    "AM_Ig_HeavyCharge": [("wFX_IceGaunt_Base_Heavy_Charging", "right_hand_attach", 0.0, None)],
    "AM_Ig_BlockStart": [("wFX_IceGaunt_Base_Block", "right_hand_attach", 0.025, None)],
    "AM_Ig_BlockIdle": [("wFX_IceGaunt_Base_Block", "right_hand_attach", 0.0, None)],
    "AM_Ig_WindIntro": [("wfxo_IceGaunt_Tempest_WindChill_Srt", "right_hand_attach", 0.0, None)],
    "AM_Ig_WindAim": [("wfxo_IceGaunt_Tempest_WindChill", "aim_direction", 0.0, None),
                      ("wfxo_IceGaunt_Tempest_WindChill_Muzzle", "right_hand_attach", 0.0, None)],
    "AM_Ig_WindOutro": [("wfxo_IceGaunt_Tempest_WindChill_End", "right_hand_attach", 0.0, None)],
    "AM_Ig_Spikes": [("wfxo_IceGaunt_Tempest_IceSpike_Cast", "", 0.15, None)],
    "AM_Ig_StormCast": [("wfxo_IceGaunt_Tempest_Blizzard_Cast", "right_hand_attach", 0.275, None)],
    # AM_Ig_Pylon: IcePylon_Hands fica de fora (a torre cresce de 0 a 1 na mão; sem curva de tamanho de malha ela
    # aparecia inteira e duplicava a torre).
    "AM_Ig_Shower": [("wfxo_IceGuant_Builder_IceShower_Casting", "right_hand_attach", 0.0, None)],
    "AM_Ig_TombIntro": [("wfxo_IceGuant_Builder_Entombed_Srt", "Spine2", 0.0, None)],
    "AM_Ig_TombOutro": [("wfxo_IceGuant_Builder_Entombed_Stop", "", 0.0, None)],
    "AM_Ig_TombBreak": [("wfxo_IceGuant_Builder_Entombed_Breakout", "", 0.0, None)],
}
lib = unreal.AnimationLibrary
done = 0
for montage, effects in MAP.items():
    am = unreal.load_asset(f"{ANIMS}/{montage}.{montage}")
    if not am:
        unreal.log_warning(f"FX: montagem ausente {montage}")
        continue
    if lib.is_valid_anim_notify_track_name(am, "FX"):
        lib.remove_animation_notify_track(am, "FX")
    lib.add_animation_notify_track(am, "FX")
    length = am.get_play_length()
    for name, bone, t0, t1 in effects:
        system = unreal.load_asset(FX.format(name))
        if not system:
            unreal.log_warning(f"FX: sistema ausente {name}")
            continue
        end = length if t1 is None else min(t1, length)
        state = lib.add_animation_notify_state_event(am, "FX", min(t0, length - 0.01), max(end - t0, 0.05),
                                                     unreal.AnimNotifyState_TimedNiagaraEffect)
        state.set_editor_property("template", system)
        state.set_editor_property("socket_name", bone)
        done += 1
    unreal.EditorAssetLibrary.save_loaded_asset(am)
unreal.log(f"FX: {done} efeitos em {len(MAP)} montagens")
