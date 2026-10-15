#!/usr/bin/env python3
"""IA das criaturas como no jogo: árvore de comportamento do Server.pak + perfil do slice → Data/AI/<conjunto>.json.

    python3 tools/make_ai.py

Fontes (docs/ia-criaturas.md, docs/ghidra.md):
- árvore: extract/serverpak/json/sharedassets/ai/behaviortrees/**/<x>_base.behaviortree.json (nwbt --x-objects json);
  TreeReference já traz a subárvore embutida;
- slice da criatura: extract/serverpak/client/slices/characters/<x>.dynamicslice.json — BehaviorTreeComponent (árvore,
  replanejamento), BlackboardComponent (m_configparams: leash, velocidades), PerceptionComponent (visão ambiente e de
  combate, aggro de grupo), PathingComponent (NavConfiguration por tipo: velocidade, giro, chegada), AITargetSelector
  (perfil de alvo: default_melee, passive...).

Saída por conjunto:
  {"profile": {...}, "tasks": {tarefa CAGE: {...}}, "tree": nó}
  nó = {"t": tipo, "n": nome, "u": utilidade?, "ni": true se não interrompível, "p": {parâmetros}, "c": [filhos]}
  parâmetro de blackboard = "$Chave"; grupo de condições = {"or": bool, "c": [[esq, op, dir], ...]} com operandos
  {"k": chave} ou {"v": valor}; op do jogo: 1 ==, 2 !=, 3 <, 4 >, 5 <=, 6 >=.
Interpretado por UNWBehaviorTreeComponent (C++).

As tarefas CAGE viram ações do port pela tabela TASKS (montagem da criatura) ou comportamentos genéricos (esperar,
virar para o alvo, levantar-se do atordoamento).
"""
import json
import os
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BT = os.path.join(ROOT, "extract", "serverpak", "json")
SLICES = os.path.join(ROOT, "extract", "serverpak", "client", "slices", "characters")
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "AI")

# conjunto do port -> slice da criatura (a árvore sai do BehaviorTreeComponent do slice)
CREATURES = {"wolf": "wolf_grey", "boar": "boar"}
# Tag de variante do Mannequin que o actionlist do jogo marca no spawn (não está no TagComponent do slice): a árvore do
# lobo escolhe o combate pelo tipo (Grey/Timber/White). Lobos timber do port usam o combate do cinza por enquanto.
VARIANT_TAGS = {"wolf": ["Grey"]}

# Tarefa CAGE -> o que o port faz. action = linha da tabela de ações da criatura; wait = espera (s, min-max);
# turn = vira para o alvo; stun = espera sair do atordoamento; tag = tag do Mannequin que o actionlist marca ao
# terminar (Alert marca "Alerted", que libera a árvore de combate).
TASKS = {
    "wolf": {
        "Bite_CAGE_Task": {"action": "Attack1"}, "Bite_Combo_CAGE_Task": {"action": "Attack2"},
        "Lunge_CAGE_Task": {"action": "Heavy"}, "Lunge_Run_CAGE_Task": {"action": "Leap"}, "Pounce_CAGE_Task": {"action": "Leap"},
        "Hamstring_CAGE_Task": {"action": "Attack1"},
        "Dodge_B_CAGE_Task": {"step": [-1, 0]}, "Dodge_L_CAGE_Task": {"step": [0, -1]}, "Dodge_R_CAGE_Task": {"step": [0, 1]},
        "Alert_CAGE_Task": {"action": "Aggro", "tag": "Alerted"}, "Alert_Turn_CAGE_Task": {"turn": True},
        "Combat_Turn_CAGE_Task": {"turn": True},
    },
    "boar": {
        "Attack_Gore_CAGE_Task": {"action": "Attack1"}, "Attack_Gore_Combo_CAGE_Task": {"action": "Attack1"},
        "Attack_Charge_Start_CAGE_Task": {"action": "ChargeStart"}, "Attack_Charge_CAGE_Task": {"action": "Heavy", "advance": True},
        "Attack_Charge_End_CAGE_Task": {"action": "ChargeEnd"}, "Pounce_CAGE_Task": {"action": "Heavy"},
        "Alert_CAGE_Task": {"action": "Aggro", "tag": "Alerted"}, "Combat_Turn_CAGE_Task": {"turn": True},
    },
}
# Comuns a todos: idle e fidget esperam; atordoamento espera o combate; reset/despawn voltam para casa.
COMMON_TASKS = {
    "Idle_CAGE_Task": {"wait": [2, 4]}, "Fidget_CAGE_Task": {"wait": [1, 2]}, "Combat_Idle_CAGE_Task": {"wait": [0.5, 1.0]},
    "Combat_Fidget_CAGE_Task": {"wait": [0.5, 1.0]}, "Reset_Idle_CAGE_Task": {"wait": [0.5, 1.0]},
    "Alert_Lay_CAGE_Task": {"wait": [1, 2]}, "Alert_Lay_OnSide_CAGE_Task": {"wait": [1, 2]}, "Alert_Sit_CAGE_Task": {"wait": [1, 2]},
    "Alert_Wallow_CAGE_Task": {"wait": [1, 2]}, "POI_Waypoint_CAGE_Task": {"wait": [2, 5]}, "POI_Waypoint_Loop_CAGE_Task": {"wait": [4, 8]},
    "Stun_CAGE_Task": {"stun": True}, "Despawn_CAGE_Task": {"home": True}, "DeathsDoorFinisherCAGETask": {"wait": [0, 0]},
}

SKIP = {"__type", "baseclass1", "child", "children", "id", "name", "baseutility", "interruptable", "serializedtree"}


def operand(p):
    if not isinstance(p, dict):
        return {"v": p}
    k = p.get("blackboardkeyname")
    if k:
        return {"k": k}
    return {"v": p.get("storage", {}).get("m_data")}


def group(g):
    el = g.get("conditions", {}).get("element")
    if el is None:
        return None
    if isinstance(el, dict):
        el = [el]
    return {"or": g.get("grouplogicaloperator", 0) == 1,
            "c": [[operand(c["leftoperand"]), c["operator"], operand(c["rightoperand"])] for c in el]}


def flat(o, pre="", out=None):
    """Parâmetros não padrão do nó, achatados com ponto (selectpositionparams.rangemax...)."""
    out = {} if out is None else out
    if not isinstance(o, dict):
        return out
    for k, v in o.items():
        if k in SKIP:
            continue
        if isinstance(v, dict):
            t = v.get("__type")
            if t == "ConditionGroup":
                g = group(v)
                if g:
                    out[pre + k.replace("conditions", "")] = g
            elif t == "BehaviorParameter":
                if v.get("blackboardkeyname"):
                    out[pre + k] = "$" + v["blackboardkeyname"]
                elif not v.get("isdefaultvalue"):
                    out[pre + k] = v.get("storage", {}).get("m_data")
            elif t == "Crc32":
                continue
            else:
                flat(v, pre + k + ".", out)
        elif isinstance(v, list):
            continue
        elif v not in (None, "", "0", "00000000-0000-0000-0000-000000000000") and k not in ("guid", "subid", "type", "hint"):
            out[pre + k] = v
    return out


def node(w):
    e = w.get("element") if isinstance(w, dict) else None
    if not e:
        return None
    types, ta, params, child, children, sub = [], None, {}, None, None, None
    while isinstance(e, dict):
        t = e.get("__type")
        types.append(t)
        if t == "TreeAsset":
            ta = e
            break
        for k, v in flat(e).items():
            params.setdefault(k, v)
        child = e.get("child", child)
        children = e.get("children", children)
        sub = e.get("serializedtree", sub)
        e = e.get("baseclass1")
    out = {"t": types[0], "n": (ta or {}).get("name", "")}
    u = (ta or {}).get("baseutility") or 0
    if u > 1e-6:
        out["u"] = round(u, 4)
    if ta and ta.get("interruptable") is False:
        out["ni"] = True
    if params:
        out["p"] = params
    kids = []
    if sub:
        kids.append(node(sub.get("element", {}).get("root")))
    if child:
        kids.append(node(child))
    if children:
        el = children.get("element", [])
        for c in el if isinstance(el, list) else [el]:
            kids.append(node(c))
    kids = [k for k in kids if k]
    if kids:
        out["c"] = kids
    return out


def find(o, t, out):
    if isinstance(o, dict):
        if o.get("__type") == t:
            out.append(o)
        for v in o.values():
            find(v, t, out)
    elif isinstance(o, list):
        for v in o:
            find(v, t, out)
    return out


def profile(slice_json):
    """Perfil do slice: visão, aggro de grupo, alvo, replanejamento, blackboard inicial, navegação por tipo."""
    p = {}
    perc = find(slice_json, "PerceptionComponentServerFacet", [])
    if perc:
        q = perc[0]
        p["ambient_vision"] = [q["m_ambientvisionprofile"]["m_visionangle"], q["m_ambientvisionprofile"]["m_visiondistance"]]
        p["combat_vision"] = [q["m_combatvisionprofile"]["m_visionangle"], q["m_combatvisionprofile"]["m_visiondistance"]]
        p["group_aggro"] = [q.get("m_groupaggrodistance", 0), q.get("m_groupaggrosharecap", 0)]
    sel = find(slice_json, "AITargetSelectorComponentServerFacet", [])
    if sel:
        p["targeting"] = os.path.basename(sel[0]["m_targetingprofile"].get("hint", "")).split(".")[0]
    btc = find(slice_json, "BehaviorTreeComponent", [])
    if btc:
        p["replan_seconds"] = btc[0].get("m_autoevaluationfrequency", 1)
        keys = btc[0].get("m_autoevaluateblackboardvariables", {}).get("element", [])
        p["replan_keys"] = keys if isinstance(keys, list) else [keys]
        ref = btc[0]["m_behaviortreeassets"]["element"]
        p["tree"] = (ref[0] if isinstance(ref, list) else ref)["m_assetref"]["hint"]
    bb = find(slice_json, "BlackboardComponentServerFacet", [])
    p["blackboard"] = {}
    for el in (bb[0]["m_configparams"]["element"] if bb else []):
        e = el.get("element", {})
        name = e.get("baseclass1", {}).get("m_name")
        if name:
            p["blackboard"][name] = e.get("m_value")
    nav = find(slice_json, "PathingComponentServerFacet", [])
    p["nav"] = {}
    for c in (nav[0]["m_navigationprofile"]["m_navconfigs"]["element"] if nav else []):
        p["nav"][c["m_navconfigtype"]] = {"max": c.get("m_maxspeed"), "min": c.get("m_minspeed"), "turn": c.get("m_turnratedegs"),
                                         "arrive": c.get("m_arrivalthreshold"), "repath": c.get("m_targetrepathdistance"),
                                         # destino padrão do tipo (nó sem SelectPositionParams próprios)
                                         "select": flat(c.get("m_selectpositionparams", {}))}
    return p


def main():
    os.makedirs(OUT, exist_ok=True)
    for set_name, slice_name in CREATURES.items():
        sl = json.load(open(os.path.join(SLICES, slice_name + ".dynamicslice.json")))
        prof = profile(sl)
        tree_path = os.path.join(BT, prof["tree"].lower() + ".json")
        tree = node(json.load(open(tree_path))["root"])
        # Tags do personagem (TagComponent do slice, CRC32 do nome em minúsculas): resolve pelos nomes que a árvore testa.
        used = set()

        def tags_of(x):
            if x["t"] in ("MannequinTagCondition", "SetMannequinTag") and isinstance(x.get("p", {}).get("tag"), str):
                used.add(x["p"]["tag"])
            for c in x.get("c", []):
                tags_of(c)
        tags_of(tree)
        crcs = {t["value"] for tc in find(sl, "TagComponent", []) for t in
                (tc["tags"]["element"] if isinstance(tc["tags"].get("element"), list) else [tc["tags"].get("element")]) if t}
        prof["tags"] = sorted({t for t in used if zlib.crc32(t.lower().encode()) in crcs} | set(VARIANT_TAGS.get(set_name, [])))
        tasks = dict(COMMON_TASKS)
        tasks.update(TASKS.get(set_name, {}))
        json.dump({"profile": prof, "tasks": tasks, "tree": tree}, open(os.path.join(OUT, set_name + ".json"), "w"),
                  separators=(",", ":"))
        n = [0]

        def count(x):
            n[0] += 1
            for c in x.get("c", []):
                count(c)
        count(tree)
        print(f"{set_name}: {n[0]} nós, tags {prof['tags']}, alvo {prof.get('targeting')}, visão {prof.get('ambient_vision')}"
              f" / {prof.get('combat_vision')}, nav {sorted(prof['nav'])}")


if __name__ == "__main__":
    main()
