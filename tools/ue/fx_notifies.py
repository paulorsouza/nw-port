"""Hand and body effects of the Ice Gauntlet on the AM_Ig_* montages (ParticleEffect procclips from the .adb, converted
by tools/ue/import_fx.py). "Timed Niagara Effect" notify state on the procclip bone: every client sees it in animation time.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script=tools/ue/fx_notifies.py

Run again after reimporting the set (the import rebuilds the montages). The "FX" track is rebuilt on every run.
"""
import unreal

ANIMS = "/Game/NW/sword_shield/Anims"
FX = "/Game/NW/FX/NS_{0}.NS_{0}"
# montage: [(effect, bone, start s, end s or None = end of the montage)] — procclip fragment and time
# (player_magicgauntlet_ice.adb); the game's "Xform" = character root (no bone).
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
    # AM_Ig_Pylon: IcePylon_Hands is left out (the tower grows from 0 to 1 in the hand; without a mesh size curve it
    # showed up whole and duplicated the tower).
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
        unreal.log_warning(f"FX: missing montage {montage}")
        continue
    if lib.is_valid_anim_notify_track_name(am, "FX"):
        lib.remove_animation_notify_track(am, "FX")
    lib.add_animation_notify_track(am, "FX")
    length = am.get_play_length()
    for name, bone, t0, t1 in effects:
        system = unreal.load_asset(FX.format(name))
        if not system:
            unreal.log_warning(f"FX: missing system {name}")
            continue
        end = length if t1 is None else min(t1, length)
        state = lib.add_animation_notify_state_event(am, "FX", min(t0, length - 0.01), max(end - t0, 0.05),
                                                     unreal.AnimNotifyState_TimedNiagaraEffect)
        state.set_editor_property("template", system)
        state.set_editor_property("socket_name", bone)
        done += 1
    unreal.EditorAssetLibrary.save_loaded_asset(am)
unreal.log(f"FX: {done} effects on {len(MAP)} montages")
