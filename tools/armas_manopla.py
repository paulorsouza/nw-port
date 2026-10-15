"""Ações da manopla de gelo para o make_datatables.py (sub-esteira 1a, docs/armas/manopla-gelo.md).

Montagens AM_Ig_* (tools/sets/player/armas_manopla-gelo.json). O terceiro campo da ação é o CostID: dá a stamina
(javelindata_staminacosts_player) e a mana (javelindata_manacosts_player).
"""

# Ataque básico (Attack_Start_Charge_UpperBody [MagicGauntlet+Ice]): soltar antes de 0,667 s = Primary (Light1);
# 0,667 s = Heavy (HeavyCharged, carregado automático do port). Os dois custam 0 de mana.
# Bloqueio e esquiva com a pose da manopla; reação ao bloqueio e quebra de guarda reaproveitam as da machadinha.
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
    # Habilidades (playeractions_ability_icemagic): animação aqui, mecânica no UNWIceGauntletComponent
    # (NWArmaManopla.cpp). Mana pelo CostID; recarga pelo ICEGAUNTLET_ABILITY_IDS (Torre e Sepultado começam no fim).
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
# Projéteis (rangedattackdatabase.radb, perfil IceMagic; slices items/weapons/icemagic_*_proj): 100 m/s
# (m_basespeed), vida 0,5 s (m_flyinglifetime) = 50 m (m_maxrange), raio 0,15 m (Primary) / 0,33 m (Heavy),
# reta por 100 m (m_projectileacceldelaydist): sem queda dentro do alcance. Disparo: CAGE-CastSpell em 0,167 s.
ICE_PROJECTILE = {"ProjectileSpeed": 10000, "ProjectileRadius": 15, "GravityDelay": 10000, "FireTimes": "0.167"}
# Prefabs IceMagic_Primary_proj / _Heavy_proj: wFX_IceGaunt_Base.Proj_Trail / Proj_Trail_Heavy (tools/ue/import_fx.py).
FX = "/Game/NW/FX/NS_{0}.NS_{0}"
ICEGAUNTLET_SHAPES = {"Light1": ("Projectile", 5000, 0), "HeavyCharged": ("Projectile", 5000, 0)}
ICEGAUNTLET_ABILITY_IDS = {"Spikes": "IceMagic_Spikes", "IceStorm": "IceMagic_IceStorm", "WindChill": "IceMagic_WindChill",
                           "IceShower": "IceMagic_IceShower"}
ICEGAUNTLET_EFFECTS = {
    "Light1": {**ICE_PROJECTILE, "ProjectileFx": FX.format("wFX_IceGaunt_Base_Proj_Trail")},
    # IceMagic_Heavy aplica IceMagic_HeavyAttackSlow (−10% de velocidade, 3 s; javelindata_statuseffects_icemagic).
    "HeavyCharged": {**ICE_PROJECTILE, "ProjectileRadius": 33, "StatusID": "IceMagic_HeavyAttackSlow",
                     "ProjectileFx": FX.format("wFX_IceGaunt_Base_Proj_Trail_Heavy"),
                     "StatusSeconds": 3, "StatusTick": 1, "StatusMoveSpeed": -0.1},
    # Fases: intro → loop (NextAction dele mesmo; o componente encerra pelo tempo) → saída.
    "Spikes": {"NextAction": "SpikesOut", "AttackAction": "SpikesEnd"},
    "WindChill": {"NextAction": "WindChillAim"},
    "WindChillAim": {"NextAction": "WindChillAim"},
    "Entombed": {"NextAction": "EntombedLoop"},
    "EntombedLoop": {"NextAction": "EntombedLoop", "AttackAction": "EntombedBreak"},
}
# Janelas do .adb que liberam cada ação no meio de outra (CAGE-Transition* de Ability_IceMagic_Primary/Heavy).
# Clique solto antes da janela fica guardado (BufferedAction) e o leve sai quando ela abrir, como o
# CAGE-TransitionBuffered do jogo.
ICEGAUNTLET_TRANSITIONS = {"HeavyCharge": "CanAttack_Light_Magic", "Light1": "CanAttack_Light_Magic",
                           **{a: f"CanAttack_{a}" for a in ("Spikes", "IceStorm", "WindChill", "Entombed", "IceShower", "IcePylon")},
                           # Ataque básico durante as estacas (0,267–0,633 s) e dentro da tumba.
                           "SpikesEnd": "Spikes_EarlyTrigger", "EntombedBreak": "Entombed_Break"}
# Empurrão do Vento Arrepiante: a linha tem ImpactDistanceY 15 com NoReaction; no port isso empurraria 15 m por
# tique (11 tiques) e o texto do jogo diz 5 m no total. Zerado até manopla-gelo-004.
ICEGAUNTLET_DAMAGE_OVERRIDES = {r: {"ImpactDistance": 0, "ImpactDistance_AI": 0, "CritImpactDistance": 0}
                                for r in ("IceMagic_WindChill", "IceMagic_WindChill_Shatter")}
