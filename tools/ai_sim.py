#!/usr/bin/env python3
"""Simulates the planning of a creature's tree (Data/AI/<set>.json) for a given blackboard: shows what it would
choose to do. Same logic as UNWBehaviorTreeComponent::Evaluate (C++), to check how the trees are read without
opening Unreal.

    python3 tools/ai_sim.py wolf Target_Dist=5 Has_Target=1
    python3 tools/ai_sim.py boar Has_Target=0 --cooldown CloseAttack
    python3 tools/ai_sim.py wolf Target_Dist=10 --vezes 20      # UtilitySelector random draws
"""
import argparse
import collections
import json
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("set")
    ap.add_argument("bb", nargs="*", help="Key=value (numbers)")
    ap.add_argument("--cooldown", action="append", default=[], help="active cooldown")
    ap.add_argument("--vezes", type=int, default=1)
    ap.add_argument("--tag", action="append", default=[], help="active Mannequin tag (e.g. Alerted)")
    ap.add_argument("--trace", action="store_true", help="show the node path to each task")
    a = ap.parse_args()
    d = json.load(open(os.path.join(ROOT, "unreal", "NWPort", "Data", "AI", a.set + ".json")))
    tasks = d["tasks"]
    bb = {k: (v if isinstance(v, (int, float)) else (1 if v is True else 0 if v is False else v))
          for k, v in d["profile"]["blackboard"].items()}
    bb.update({"Spawn_Pos": 0.0, "Difficulty_Tier": 2, "DoesLeash": 1, "Leash_Opt_Out": 0, "Has_Target": 1,
               "Target_Dist": 5, "Target_Angle": 0, "Orientation_Delta": 180, "Target_Vitals_State": 4, "Target_Reachable": 1,
               "Target_Directly_Reachable": 1, "Target_Visible": 1, "Full_Body_Reaction": 0, "Target_Pos": 5.0})
    for kv in a.bb:
        k, v = kv.split("=")
        bb[k] = float(v)
    if bb.get("Has_Target", 1) == 0:
        bb.pop("Target_Pos", None)
    cooldowns = set(a.cooldown)
    a.tag += d["profile"].get("tags", [])

    def operand(o):
        if "k" in o:
            v = bb.get(o["k"], 0)
            return v if isinstance(v, (int, float)) else 0
        v = o.get("v")
        if isinstance(v, bool):
            return 1 if v else 0
        if isinstance(v, str):
            return 1 if v.lower() == "true" else 0 if v.lower() == "false" else float(v or 0)
        return v or 0

    def cmp(l, op, r):
        return {1: abs(l - r) < 1e-4, 2: abs(l - r) >= 1e-4, 3: l < r, 4: l > r, 5: l <= r, 6: l >= r}.get(op, False)

    def group(n, g):
        G = n.get("p", {}).get(g)
        if not G:
            return True
        def left(c):
            # "position == True": the key exists.
            if isinstance(c[2].get("v"), bool) and "k" in c[0]:
                v = bb.get(c[0]["k"])
                return 1 if v not in (None, 0, False) else 0
            return operand(c[0])
        res = [cmp(left(c), c[1], operand(c[2])) for c in G["c"]]
        return any(res) if G["or"] else all(res)

    def num(n, k, dft=0):
        v = n.get("p", {}).get(k, dft)
        if isinstance(v, str) and v.startswith("$"):
            v = bb.get(v[1:], dft)
        return v if isinstance(v, (int, float)) else dft

    STACK = []

    def ev(n):
        STACK.append(n["n"])
        try:
            return ev_(n)
        finally:
            STACK.pop()

    def ev_(n):
        """(plan as a list of leaves, utility) or None."""
        t, kids, u = n["t"], n.get("c", []), n.get("u", 0)
        p = n.get("p", {})
        if t in ("Selector", "FallbackSelector"):
            for c in kids:
                r = ev(c)
                if r:
                    return r[0], max(u, r[1])
            return None
        if t in ("Sequencer", "Parallel", "PermissiveSequencer"):
            plan, best = [], u
            for c in kids:
                r = ev(c)
                if not r:
                    if t == "PermissiveSequencer":
                        continue
                    return None
                plan += r[0]
                best = max(best, r[1])
            return (plan, best) if plan else None
        if t == "UtilitySelector":
            cands = [r for r in (ev(c) for c in kids) if r]
            if not cands:
                return None
            if num(n, "selectionmethod") == 1:
                tot = sum(max(r[1], 1e-4) for r in cands)
                x = random.uniform(0, tot)
                for r in cands:
                    x -= max(r[1], 1e-4)
                    if x <= 0:
                        return r[0], max(u, r[1])
                return cands[-1][0], max(u, cands[-1][1])
            r = max(cands, key=lambda r: r[1])
            return r[0], max(u, r[1])

        def child():
            r = ev(kids[0]) if kids else None
            return (r[0], u if u > 1e-5 else r[1]) if r else None
        if t in ("BlackboardCondition", "DistanceCondition"):
            return child() if group(n, "entry") else None
        if t == "VariableExistsCondition":
            return child() if ((p.get("variablename") in bb) != bool(p.get("invertcheck"))) else None
        if t == "IsCooldownActiveCondition":
            return child() if ((p.get("cooldownname") in cooldowns) != bool(p.get("invertcheck"))) else None
        if t == "MannequinTagCondition":
            return child() if ((p.get("tag") in a.tag) != bool(p.get("invertcheck"))) else None
        if t == "CooldownDecorator":
            return child() if p.get("cooldownname") not in cooldowns else None
        if t == "AlwaysSucceed":
            r = ev(kids[0]) if kids else None
            return (r[0] if r else [], u)
        if kids:
            return child()
        if t == "CAGEAction":
            task = p.get("actionname", "")
            if isinstance(task, str) and task.startswith("$"):
                task = bb.get(task[1:], task)
            m = tasks.get(task, {})
            if "action" in m and not bb.get("Has_Target"):
                return None
            return [f"CAGE {task} -> {m or 'short wait'}" + (f"  [{' > '.join(STACK[-6:])}]" if a.trace else "")], u
        if t == "NavigationAction":
            o = p.get("selectpositionparams.origin.positiontype", 1)
            if o in (1, 2, 12) and not bb.get("Has_Target"):
                return None
            return [f"NAV {n['n']} ({p.get('navconfigtype')}, origin {o}, {p.get('selectpositionparams.rangemin', '')}-"
                    f"{p.get('selectpositionparams.rangemax', '')} m)"], u
        return [f"{t} {n['n']} {p.get('cooldownname', '') or p.get('factname', '')}"], u

    counts = collections.Counter()
    for _ in range(a.vezes):
        r = ev(d["tree"])
        counts[" | ".join(r[0]) if r else "(nothing evaluates)"] += 1
    for plan, c in counts.most_common():
        print(f"{c}x  {plan}")


if __name__ == "__main__":
    main()
