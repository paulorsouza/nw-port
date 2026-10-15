"""Ice Gauntlet actions for make_datatables.py (sub-area 1a, docs/game/weapons/ice-gauntlet.md).

AM_Ig_* montages (tools/sets/player/armas_manopla-gelo.json). The third field of an action is the CostID: it gives the
stamina (javelindata_staminacosts_player) and the mana (javelindata_manacosts_player).
"""

# Basic attack (Attack_Start_Charge_UpperBody [MagicGauntlet+Ice]): release before 0.667 s = Primary (Light1);
# 0.667 s = Heavy (HeavyCharged, auto-charged in the port). Both cost 0 mana.
# Block and dodge use the gauntlet pose; block reaction and guard break reuse the Hatchet ones.
ICEGAUNTLET_ACTIONS = {
    "Light1": ("AM_Ig_Primary", "IceMagic_Primary", "IceMagic_Primary", ""),
    "HeavyCharge": ("AM_Ig_HeavyCharge", "", "", ""),
    "HeavyCharged": ("AM_Ig_Heavy", "IceMagic_Heavy", "IceMagic_Heavy", ""),
    **{f"Dodge_{d}": (f"AM_Ig_Dodge_{d}", "", "Dodge_Roll", "") for d in "FBLR"},
    "BlockStart": ("AM_Ig_BlockStart", "", "", ""), "BlockEnd": ("AM_Ig_BlockEnd", "", "", ""),
    "BlockIdle": ("AM_Ig_BlockIdle", "", "", ""), "BlockReact": ("AM_Hx_BlockReact", "", "", ""),
    "BlockBreak": ("AM_Hx_BlockBreak", "", "", ""),
    **{f"Hit_{d}": (f"AM_Hit_{d}", "", "", "") for d in "FBLR"},
    "Death": ("AM_Death", "", "", ""), "StunLoop": ("AM_StunLoop", "", "", ""),
    "Draw": ("AM_Ig_Draw", "", "", ""), "Sheath": ("AM_Ig_Sheath", "", "", ""),
    # Abilities (playeractions_ability_icemagic): animation here, mechanics in UNWIceGauntletComponent
    # (NWArmaManopla.cpp). Mana from the CostID; cooldown from ICEGAUNTLET_ABILITY_IDS (Pylon and Entombed start at the end).
    "Spikes": ("AM_Ig_Spikes", "", "IceMagic_Spikes", ""),
    "SpikesOut": ("AM_Ig_SpikesOut", "", "", ""),
    "SpikesEnd": ("AM_Ig_SpikesOut", "", "", ""),
    "WindChill": ("AM_Ig_WindIntro", "", "IceMagic_WindChill", ""),
    "WindChillAim": ("AM_Ig_WindAim", "", "", ""),
    "WindChillOutro": ("AM_Ig_WindOutro", "", "", ""),
    "IceStorm": ("AM_Ig_StormCast", "", "IceMagic_IceStorm", ""),
    "IceShower": ("AM_Ig_Shower", "", "IceMagic_IceShower", ""),
    "IcePylon": ("AM_Ig_Pylon", "", "IceMagic_IcePylon", ""),
    "Entombed": ("AM_Ig_TombIntro", "", "IceMagic_Entombed", ""),
    "EntombedLoop": ("AM_Ig_TombLoop", "", "", ""),
    "EntombedOutro": ("AM_Ig_TombOutro", "", "", ""),
    "EntombedBreak": ("AM_Ig_TombBreak", "", "IceMagic_Entombed_Break", ""),
}
# Projectiles (rangedattackdatabase.radb, IceMagic profile; slices items/weapons/icemagic_*_proj): 100 m/s
# (m_basespeed), lifetime 0.5 s (m_flyinglifetime) = 50 m (m_maxrange), radius 0.15 m (Primary) / 0.33 m (Heavy),
# straight for 100 m (m_projectileacceldelaydist): no drop within range. Fire: CAGE-CastSpell at 0.167 s.
ICE_PROJECTILE = {"ProjectileSpeed": 10000, "ProjectileRadius": 15, "GravityDelay": 10000, "FireTimes": "0.167"}
# IceMagic_Primary_proj / _Heavy_proj prefabs: wFX_IceGaunt_Base.Proj_Trail / Proj_Trail_Heavy (tools/ue/import_fx.py).
FX = "/Game/NW/FX/NS_{0}.NS_{0}"
ICEGAUNTLET_SHAPES = {"Light1": ("Projectile", 5000, 0), "HeavyCharged": ("Projectile", 5000, 0)}
ICEGAUNTLET_ABILITY_IDS = {"Spikes": "IceMagic_Spikes", "IceStorm": "IceMagic_IceStorm", "WindChill": "IceMagic_WindChill",
                           "IceShower": "IceMagic_IceShower"}
ICEGAUNTLET_EFFECTS = {
    "Light1": {**ICE_PROJECTILE, "ProjectileFx": FX.format("wFX_IceGaunt_Base_Proj_Trail")},
    # IceMagic_Heavy applies IceMagic_HeavyAttackSlow (−10% speed, 3 s; javelindata_statuseffects_icemagic).
    "HeavyCharged": {**ICE_PROJECTILE, "ProjectileRadius": 33, "StatusID": "IceMagic_HeavyAttackSlow",
                     "ProjectileFx": FX.format("wFX_IceGaunt_Base_Proj_Trail_Heavy"),
                     "StatusSeconds": 3, "StatusTick": 1, "StatusMoveSpeed": -0.1},
    # Phases: intro → loop (NextAction is itself; the component ends it by time) → outro.
    "Spikes": {"NextAction": "SpikesOut", "AttackAction": "SpikesEnd"},
    "WindChill": {"NextAction": "WindChillAim"},
    "WindChillAim": {"NextAction": "WindChillAim"},
    "Entombed": {"NextAction": "EntombedLoop"},
    "EntombedLoop": {"NextAction": "EntombedLoop", "AttackAction": "EntombedBreak"},
}
# .adb windows that allow each action in the middle of another (CAGE-Transition* of Ability_IceMagic_Primary/Heavy).
# A click released before the window is buffered (BufferedAction) and the light attack fires when it opens, like the
# game's CAGE-TransitionBuffered.
ICEGAUNTLET_TRANSITIONS = {"HeavyCharge": "CanAttack_Light_Magic", "Light1": "CanAttack_Light_Magic",
                           **{a: f"CanAttack_{a}" for a in ("Spikes", "IceStorm", "WindChill", "Entombed", "IceShower", "IcePylon")},
                           # Basic attack during the spikes (0.267–0.633 s) and inside the tomb.
                           "SpikesEnd": "Spikes_EarlyTrigger", "EntombedBreak": "Entombed_Break"}
# Wind Chill push: the row has ImpactDistanceY 15 with NoReaction; in the port that would push 15 m per
# tick (11 ticks) and the game text says 5 m in total. Zeroed until manopla-gelo-004.
ICEGAUNTLET_DAMAGE_OVERRIDES = {r: {"ImpactDistance": 0, "ImpactDistance_AI": 0, "CritImpactDistance": 0}
                                for r in ("IceMagic_WindChill", "IceMagic_WindChill_Shatter")}
