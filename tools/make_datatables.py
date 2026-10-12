#!/usr/bin/env python3
"""Gera os CSV das DataTables do Unreal (structs FNWDamageRow e FNWActionRow) a partir dos dados do jogo.

uso: make_datatables.py extract/ue/sword_shield/manifest.json
Saída: extract/ue/<conjunto>/data/DT_NWDamage.csv e DT_NWActions.csv (importados pelo tools/ue/import_nw.py).
Requer os datasheets em JSON em extract/data (nwbt unpack --x-datasheet json).
"""
import csv
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DT = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")

# Ação -> (montagem, DamageID do jogo, CostID de stamina do jogo, próxima do combo).
ACTIONS = {
    "Light1": ("AM_LightCombo_1", "1H_Sword_Attack1", "Attack_1H_Sword_Primary", "Light2"),
    "Light2": ("AM_LightCombo_2", "1H_Sword_Attack1", "Attack_1H_Sword_Primary", "Light3"),
    "Light3": ("AM_LightCombo_3", "1H_Sword_Attack1_Ender", "Attack_1H_Sword_Primary", ""),
    "Heavy": ("AM_Heavy", "1H_Sword_Attack2", "Attack_1H_Sword_Heavy", ""),
    # Segurar o botão: carga (Attack_Start_Charge); soltar cedo = leve, no meio = Heavy, carga cheia = HeavyCharged.
    "HeavyCharge": ("AM_HeavyCharge", "", "", ""),
    "HeavyCharged": ("AM_HeavyCharged", "1H_Sword_Attack2_Charged", "Attack_1H_Sword_Heavy", ""),
    "SprintAttack": ("AM_SprintAttack", "1H_Sword_Attack_Charge", "Attack_1H_Sword_Run", ""),
    "ShieldBash": ("AM_ShieldBash", "OH_Shield_Attack_Bash", "", ""),
    "ShieldRush": ("AM_ShieldRush", "OH_Shield_Attack_Rush", "", ""),
    "SpinAttack": ("AM_SpinAttack", "1H_Sword_Attack_Spin", "Attack_1H_Sword_Special", ""),
    # Transtorno (Ability_Sword_ReverseStab): 180% = 1H_Sword_Attack_Stab (a linha Swipe é de outra versão).
    "SwipeStab": ("AM_SwipeStab", "1H_Sword_Attack_Stab", "Attack_1H_Sword_Special", ""),
    # Golpe com Salto: mesmo clipe do golpe de corrida (fragmento Ability_Sword_Leap_*).
    "LeapingStrike": ("AM_SprintAttack", "1H_Sword_Attack_Charge", "Attack_1H_Sword_Special", ""),
    "FinalStand": ("AM_FinalStand", "", "", ""),
    "StunLoop": ("AM_StunLoop", "", "", ""),
    "Shout": ("AM_Shout", "", "", ""),
    "Dodge_F": ("AM_Dodge_F", "", "Dodge_Roll_Fast", ""),
    "Dodge_B": ("AM_Dodge_B", "", "Dodge_Roll_Fast", ""),
    "Dodge_L": ("AM_Dodge_L", "", "Dodge_Roll_Fast", ""),
    "Dodge_R": ("AM_Dodge_R", "", "Dodge_Roll_Fast", ""),
    "BlockStart": ("AM_BlockStart", "", "", ""),
    "BlockEnd": ("AM_BlockEnd", "", "", ""),
    "BlockIdle": ("AM_BlockIdle", "", "", ""),
    "BlockReact": ("AM_BlockReact", "", "", ""),
    "BlockBreak": ("AM_BlockBreak", "", "", ""),
    "Hit_F": ("AM_Hit_F", "", "", ""),
    "Hit_B": ("AM_Hit_B", "", "", ""),
    "Hit_L": ("AM_Hit_L", "", "", ""),
    "Hit_R": ("AM_Hit_R", "", "", ""),
    "Death": ("AM_Death", "", "", ""),
    "Draw": ("AM_Draw", "", "", ""),
}


# Lost: ataques do inimigo usando as linhas de dano da espada do jogador (as do NPC usam outra escala).
LOST_ACTIONS = {
    "Attack1": ("AM_Attack1", "1H_Sword_Attack1", "", ""),
    "Attack2": ("AM_Attack2", "1H_Sword_Attack1", "", ""),
    "Heavy": ("AM_Heavy", "1H_Sword_Attack2", "", ""),
    "Leap": ("AM_Leap", "1H_Sword_Attack_Spec3", "", ""),
    "Aggro": ("AM_Aggro", "", "", ""),
    "Hit_F": ("AM_Hit_F", "", "", ""),
    "Hit_B": ("AM_Hit_B", "", "", ""),
    "Hit_L": ("AM_Hit_L", "", "", ""),
    "Hit_R": ("AM_Hit_R", "", "", ""),
    "Death": ("AM_Death", "", "", ""),
}
# Formato do acerto (padrão: lâmina). Alcance em cm, ângulo = abertura total do cone. Valores iniciais, a calibrar com nw.DebugHits.
HIT_SHAPES = {
    "Heavy": ("Front", 230, 50),
    "HeavyCharged": ("Front", 230, 50),
    "SprintAttack": ("Front", 260, 70),
    "ShieldBash": ("Front", 190, 100),
    "ShieldRush": ("Front", 220, 110),
    "SpinAttack": ("Around", 260, 360),
    "SwipeStab": ("Front", 270, 80),
    "LeapingStrike": ("Front", 260, 70),
}
# Habilidade -> AbilityID do jogo (recarga em javelindata_cooldowns_player).
ABILITY_IDS = {
    "ShieldBash": "Ability_Sword_Bash",
    "ShieldRush": "Ability_Sword_Rush",
    "SpinAttack": "Ability_Sword_WhirlingBlade",
    "SwipeStab": "Ability_Sword_ReverseStab",
    "Shout": "Ability_Sword_FinalStand",
    "FinalStand": "Ability_Sword_FinalStand",
    "LeapingStrike": "Ability_Sword_LeapingStrike",
}
# Efeitos das descrições do jogo (weaponabilities.loc.xml): atordoar o alvo (s); buff em si (s, redução de dano).
EFFECTS = {
    "ShieldBash": {"TargetStun": 2.0},
    "FinalStand": {"BuffSeconds": 8.0, "BuffDamageReduction": 0.3},
}
# Machadinha (mesmo esqueleto; montagens AM_Hx_*). Reações e morte reaproveitam as da espada.
HATCHET_ACTIONS = {
    "Light1": ("AM_Hx_LightCombo_1", "Hatchet_Damage_PrimaryAttack", "", "Light2"),
    "Light2": ("AM_Hx_LightCombo_2", "Hatchet_Damage_PrimaryAttack", "", "Light3"),
    "Light3": ("AM_Hx_LightCombo_3", "Hatchet_Damage_PrimaryAttack_Ender", "", ""),
    "HeavyCharge": ("AM_Hx_HeavyCharge", "", "", ""),
    "Heavy": ("AM_Hx_Heavy", "Hatchet_Damage_HeavyAttack", "", ""),
    "HeavyCharged": ("AM_Hx_HeavyCharged", "Hatchet_Damage_HeavyAttack_Charged", "", ""),
    "SprintAttack": ("AM_Hx_SprintAttack", "Hatchet_Damage_SprintAttack1", "", ""),
    "Dodge_F": ("AM_Hx_Dodge_F", "", "Dodge_Roll_Fast", ""),
    "Dodge_B": ("AM_Hx_Dodge_B", "", "Dodge_Roll_Fast", ""),
    "Dodge_L": ("AM_Hx_Dodge_L", "", "Dodge_Roll_Fast", ""),
    "Dodge_R": ("AM_Hx_Dodge_R", "", "Dodge_Roll_Fast", ""),
    "BlockStart": ("AM_Hx_BlockStart", "", "", ""),
    "BlockEnd": ("AM_Hx_BlockEnd", "", "", ""),
    "BlockIdle": ("AM_Hx_BlockIdle", "", "", ""),
    "BlockReact": ("AM_Hx_BlockReact", "", "", ""),
    "BlockBreak": ("AM_Hx_BlockBreak", "", "", ""),
    "Hit_F": ("AM_Hit_F", "", "", ""),
    "Hit_B": ("AM_Hit_B", "", "", ""),
    "Hit_L": ("AM_Hit_L", "", "", ""),
    "Hit_R": ("AM_Hit_R", "", "", ""),
    "Death": ("AM_Death", "", "", ""),
    "StunLoop": ("AM_StunLoop", "", "", ""),
    "Draw": ("AM_Hx_Draw", "", "", ""),
    "Sheath": ("AM_Hx_Sheath", "", "", ""),
    "RagingTorrent": ("AM_Hx_RagingTorrent", "Hatchet_Damage_RapidStrikes", "", ""),
    "RendingThrow": ("AM_Hx_RendingThrow", "Hatchet_Damage_RendingThrow", "", ""),
    "SocialDistancing": ("AM_Hx_SocialDistancing", "Hatchet_Damage_SocialDistancing", "", ""),
    "InfectedThrow": ("AM_Hx_Special4", "Hatchet_Damage_InfectedThrow", "", ""),
    "FeralRush": ("AM_Hx_Special3", "Hatchet_Damage_SprintAttack1", "", ""),
    "Berserk": ("AM_Hx_Special5", "", "", ""),
}
HATCHET_SHAPES = {
    "Heavy": ("Front", 230, 60), "HeavyCharged": ("Front", 230, 60), "SprintAttack": ("Front", 260, 70),
    "FeralRush": ("Front", 260, 80), "RagingTorrent": ("Front", 230, 90),
    "RendingThrow": ("Projectile", 2500, 0), "SocialDistancing": ("Projectile", 2500, 0), "InfectedThrow": ("Projectile", 2500, 0),
}
HATCHET_ABILITY_IDS = {a: f"Ability_Hatchet_{a}" for a in
                       ("Berserk", "FeralRush", "RagingTorrent", "RendingThrow", "SocialDistancing", "InfectedThrow")}
# Berserk: +20% de dano por 5 s (descrição do jogo).
HATCHET_EFFECTS = {"Berserk": {"BuffSeconds": 5.0, "BuffDamageBonus": 0.2}}
SETS = {"sword_shield": [("DT_NWActions", ACTIONS, HIT_SHAPES, ABILITY_IDS, None),
                         ("DT_HatchetActions", HATCHET_ACTIONS, HATCHET_SHAPES, HATCHET_ABILITY_IDS, HATCHET_EFFECTS)],
        "lost_sword": [("DT_LostActions", LOST_ACTIONS, HIT_SHAPES, {}, {})]}


def rows(name):
    return json.load(open(os.path.join(DT, name + ".datasheet.json")))["rows"]


def main():
    manifest = json.load(open(sys.argv[1]))
    out = os.path.join(os.path.dirname(sys.argv[1]), "data")
    os.makedirs(out, exist_ok=True)
    anim_path = f"/Game/NW/{manifest['name']}/Anims"

    montages = set()
    for name, secs in manifest["montages"].items():
        montages |= {name} if len(secs) == 1 else {f"{name}_{i + 1}" for i in range(len(secs))}

    damage = {r["DamageID"]: r for r in rows("javelindata_damagetable")}
    stamina = {r["CostID"]: r["StaminaCost"] for r in rows("javelindata_staminacosts_player")}

    tables = SETS[manifest["name"]]
    # Tabela de dano única para todos os conjuntos: todas as linhas de espada e escudo.
    used = sorted(d for d in damage if d.startswith(("1H_Sword", "OH_Shield", "Hatchet_")))
    with open(os.path.join(out, "DT_NWDamage.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Name", "WeaponCategory", "DamageType", "DmgCoef", "HitStun", "BlockHitStun",
                    "PowerLevel", "BlockPowerLevel", "ImpactDistance", "Unblockable"])
        for did in used:
            r = damage[did]
            w.writerow([did, r["WeaponCategory"], r["DamageType"], r["DmgCoef"], r["HitStun"], r["BlockHitStun"],
                        r["PowerLevel"], r["BlockPowerLevel"], r["ImpactDistanceY"], str(bool(r["Unblockable"]))])

    cooldown = {r["AbilityID"]: r["Time"] for r in rows("javelindata_cooldowns_player")}
    missing = []
    for table_name, actions, shapes, ability_ids, effects in tables:
        if effects is None:
            effects = EFFECTS
        with open(os.path.join(out, f"{table_name}.csv"), "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["Name", "Montage", "DamageID", "StaminaCost", "ComboNext", "HitShape", "HitRange", "HitAngle", "Cooldown",
                        "TargetStun", "BuffSeconds", "BuffDamageReduction", "BuffDamageBonus"])
            for action, (montage, did, cost_id, nxt) in actions.items():
                if montage not in montages:
                    missing.append(montage)
                shape, rng, ang = shapes.get(action, ("Blade", 0, 0))
                cd = cooldown.get(ability_ids.get(action, ""), 0)
                fx = effects.get(action, {})
                w.writerow([action, f"{anim_path}/{montage}.{montage}", did, stamina.get(cost_id, 0), nxt, shape, rng, ang, cd,
                            fx.get("TargetStun", 0), fx.get("BuffSeconds", 0), fx.get("BuffDamageReduction", 0),
                            fx.get("BuffDamageBonus", 0)])
        print(f"{len(actions)} ações ({table_name})")
    print(f"{len(used)} danos em {out}")
    if missing:
        print(f"montagens ausentes no manifest: {missing}", file=sys.stderr)


if __name__ == "__main__":
    main()
