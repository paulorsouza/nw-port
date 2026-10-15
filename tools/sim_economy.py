#!/usr/bin/env python3
"""Town economy simulator (docs/game/economy.md), week by week.

    python3 tools/sim_economy.py [--param tools/economia.json] [--players N] [--weeks N] [--set a.b=value ...]

Aggregate model per town (one town per region): players run dungeons, gather, sell raw materials to the town,
buy the list (potions, catalysts) and pay tax when crafting. The governor (style in "governadores") picks
where the price sits in the range (0 = minimum, 1 = maximum), tax, buffs and how much is spent on stations and
defense. Attacks every 2 days from week 2. Output: coin created and destroyed, player income, balance sheet, tier
and prize of each governor. Price ranges from tools/precos_cidade.json; the rest from tools/economia.json.
"""
import argparse
import json
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def set_path(d, dotted, value):
    keys = dotted.split(".")
    for k in keys[:-1]:
        d = d[int(k)] if isinstance(d, list) else d[k]
    last = keys[-1]
    try:
        value = json.loads(value)
    except ValueError:
        pass
    if isinstance(d, list):
        d[int(last)] = value
    else:
        d[last] = value


def price_at(rng, pos):
    """Price (coins) at position pos (0..1) of the range."""
    return (rng["min"] + pos * (rng["max"] - rng["min"])) / 100.0


def simulate(p, prices, ref):
    J, C, PR = p["jogador"], p["cidade"], p["premio"]
    P = p["jogadores_por_regiao"]
    govs = (p["governadores"] * ((p["cidades"] + len(p["governadores"]) - 1) // len(p["governadores"])))[: p["cidades"]]
    raw = prices["compra"]
    sell = prices["venda"]
    potion_ids = [k for k in sell if k.startswith("Potion")]
    cat_ids = [k for k in sell if not k.startswith("Potion")]

    cities = []
    for g in govs:
        cities.append({"g": g, "treasury": C["balancete_inicial"], "stock": 0.0, "levels": [0] * C["mesas"], "peak": [0] * C["mesas"], "weapons": 0,
                       "hist": [], "prizes": 0, "dark": 0, "tiers": []})
    minted = {"dungeon": 0.0, "handout": 0.0, "prize": 0.0, "opening_balance": C["balancete_inicial"] * len(cities)}
    burned = {"projects": 0.0, "buffs": 0.0, "weapons": 0.0, "town_purchase": C["preco_cidade"] * len(cities)}
    weeks = []
    rnd = random.Random(p.get("semente", 1))

    hours = J["horas_semana"]
    dungeons = hours * J["fracao_dungeon"] * 60.0 / J["minutos_por_dungeon"]
    gather_units = hours * J["fracao_coleta"] * J["unidades_coleta_hora"]

    for w in range(1, p["semanas"] + 1):
        row = {"week": w, "towns": []}
        week_income = []
        for c in cities:
            g = c["g"]
            act = g["atividade"]
            # --- players: dungeon and handout (new coin)
            dung = P * dungeons * J["moedas_por_dungeon"]
            handout = P * J["brinde_moedas_semana"]
            minted["dungeon"] += dung
            minted["handout"] += handout
            # --- mean raw material buy price at the governor's position
            buy_price = sum(price_at(r, g["preco_compra"]) for r in raw.values()) / len(raw)
            ref_raw = sum(price_at(r, 0.5) for r in raw.values()) / len(raw)
            # How much the players want to sell to the town: rises with the price.
            sell_frac = min(1.0, 0.3 + 0.7 * g["preco_compra"])
            supply = P * gather_units * sell_frac
            # How much the town wants: material for each station's next level (active governor) + repairs.
            need = 0.0
            for lv in c["levels"]:
                if lv < C["nivel_max"]:
                    need += C["evoluir"][lv]["materia_prima"] * act
            need += c.get("repair", 0.0)
            # Upkeep from use: each craft on an upgraded station spends raw material (grows with the players).
            avg_lv0 = sum(c["levels"]) / len(c["levels"])
            use = P * min(1.0, 0.6 + 0.1 * avg_lv0 + 0.05 * g["buffs"])
            upkeep = avg_lv0 * use * C["manutencao_por_jogador_nivel"]
            repair_need = c.get("repair", 0.0)
            need += upkeep
            need = max(0.0, need - c["stock"])
            if act == 0:
                need = 0.0
            afford = c["treasury"] / buy_price if buy_price > 0 else 0
            units = min(supply, need, afford, P * C["limite_compra_por_tipo"] * len(raw))
            raw_cost = units * buy_price
            # The part of the purchase that went to repairs does not count toward the goal.
            raw_counted = raw_cost * (1.0 - min(1.0, repair_need / need)) if need > 0 else 0.0
            c["treasury"] -= raw_cost
            c["stock"] += units
            # Pays the upkeep; without material, a station loses a level.
            if c["stock"] >= upkeep:
                c["stock"] -= upkeep
            elif upkeep > 0:
                c["stock"] = 0.0
                j = max(range(len(c["levels"])), key=lambda k: c["levels"][k])
                c["levels"][j] -= 1
            # --- list: potions and catalysts, price-elastic demand
            def demand(ids, base):
                q = rev = 0.0
                for i in ids:
                    pr = price_at(sell[i], g["preco_venda"])
                    rf = price_at(sell[i], 0.5)
                    qi = min(P * C["limite_venda_por_tipo"], P * base / len(ids) * (rf / pr) ** J["elasticidade"])
                    q += qi
                    rev += qi * pr
                return q, rev
            q_pot, r_pot = demand(potion_ids, J["pocoes_semana"])
            q_cat, r_cat = demand(cat_ids, J["catalisadores_semana"])
            list_q, list_rev = q_pot + q_cat, r_pot + r_cat
            c["treasury"] += list_rev
            # --- tax: share of crafting in the town (better stations and buffs attract, high tax drives away)
            avg_lv = sum(c["levels"]) / len(c["levels"])
            share = min(1.0, 0.6 + 0.1 * avg_lv + 0.05 * g["buffs"]) * max(0.2, 1.0 - (g["imposto"] - C["imposto_padrao"]) * 4)
            tax = P * J["valor_fabricado_semana"] * share * g["imposto"]
            c["treasury"] += tax
            # --- governor spending: buffs, defense, upgrades
            buffs = 0
            for _ in range(g["buffs"]):
                if c["treasury"] >= C["buff_moedas"]:
                    c["treasury"] -= C["buff_moedas"]
                    burned["buffs"] += C["buff_moedas"]
                    buffs += 1
            proj = buffs * C["buff_moedas"]
            strength = C["ataque_base"] + C["ataque_por_semana"] * w + C["ataque_por_nivel_cidade"] * sum(c["levels"])
            if act > 0:
                while c["weapons"] < strength * g.get("margem_defesa", 1.0) and c["treasury"] >= C["arma_moedas"]:
                    c["treasury"] -= C["arma_moedas"]
                    burned["weapons"] += C["arma_moedas"]
                    proj += C["arma_moedas"]
                    c["weapons"] += 1
            c["repair"] = 0.0
            for i, lv in enumerate(c["levels"]):
                if lv >= C["nivel_max"] or act == 0:
                    continue
                e = C["evoluir"][lv]
                if c["stock"] >= e["materia_prima"] and c["treasury"] >= e["moedas"]:
                    c["stock"] -= e["materia_prima"]
                    c["treasury"] -= e["moedas"]
                    burned["projects"] += e["moedas"]
                    c["levels"][i] += 1
                    # Only a level reached for the first time counts toward the goal (rebuilding after an attack does not).
                    if c["levels"][i] > c["peak"][i]:
                        c["peak"][i] = c["levels"][i]
                        proj += e["moedas"] + e["materia_prima"] * ref_raw
            # --- attacks (from week 2, ~3.5 per week)
            lost = 0
            if w >= 2 and act >= 0:
                for _ in range(3 if w % 2 else 4):
                    # Attack strength varies ±30%: a just-enough defense sometimes loses.
                    if c["weapons"] < strength * rnd.uniform(0.7, 1.3):
                        for _ in range(C["mesas_perdidas_por_ataque"]):
                            j = max(range(len(c["levels"])), key=lambda k: c["levels"][k])
                            if c["levels"][j] > 0:
                                c["levels"][j] -= 1
                                c["repair"] += C["evoluir"][c["levels"][j]]["materia_prima"] * C["conserto_fracao"]
                                lost += 1
                        c["weapons"] = max(0, c["weapons"] - 1)
            # --- score and income
            # What goes to the players (raw material purchases) weighs more than what comes from them (list, tax).
            score = raw_counted + PR["peso_lista"] * list_rev + PR["peso_imposto"] * tax + proj \
                + PR["peso_nivel"] * sum(c["levels"]) / len(c["levels"])
            dark = units < PR["tenebroso_materia_prima"] or list_q < PR["tenebroso_lista"] or buffs == 0
            income = (dung + handout + raw_cost - list_rev - tax) / P
            week_income.append(income)
            c["hist"].append(score)
            row["towns"].append({"name": g["nome"], "treasury": c["treasury"], "stock": c["stock"], "raw_bought": units,
                                   "list": list_q, "tax": tax, "mean_level": sum(c["levels"]) / len(c["levels"]),
                                   "weapons": c["weapons"], "attack_strength": strength, "stations_lost": lost,
                                   "score": score, "dark": dark, "player_income": income})
        # --- prize tiers: ratio to the week's town average
        scores = [x["score"] for x in row["towns"]]
        # Average only over towns not in the "tenebroso" tier (an absent one does not inflate the others).
        alive = [x["score"] for x in row["towns"] if not x["dark"]] or scores
        mean = sum(alive) / len(alive) if alive else 1
        best = max(range(len(scores)), key=lambda k: scores[k])
        for k, x in enumerate(row["towns"]):
            r = x["score"] / mean if mean > 0 else 0
            if x["dark"]:
                tier = "tenebroso"
            elif r >= PR["faixas"]["excelente"]:
                tier = "melhor" if k == best else "excelente"
            elif r >= PR["faixas"]["muito_bom"]:
                tier = "muito_bom"
            elif r >= PR["faixas"]["bom"]:
                tier = "bom"
            else:
                tier = "ruim"
            x["ratio"] = r
            x["tier"] = tier
            prize = PR[tier]
            minted["prize"] += prize
            cities[k]["prizes"] += prize
            cities[k]["tiers"].append(tier)
        row["mean_income"] = sum(week_income) / len(week_income)
        weeks.append(row)
    return weeks, cities, minted, burned


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--param", default=os.path.join(ROOT, "tools", "economia.json"))
    ap.add_argument("--players", type=int)
    ap.add_argument("--weeks", type=int)
    ap.add_argument("--set", action="append", default=[], help="a.b=value (JSON) overrides a parameter")
    ap.add_argument("--detail", action="store_true", help="per-town table for each week")
    # Old spellings, still accepted (hidden from --help).
    ap.add_argument("--jogadores", dest="players", type=int, help=argparse.SUPPRESS)
    ap.add_argument("--semanas", dest="weeks", type=int, help=argparse.SUPPRESS)
    ap.add_argument("--detalhe", dest="detail", action="store_true", help=argparse.SUPPRESS)
    a = ap.parse_args()
    p = load(a.param)
    if a.players:
        p["jogadores_por_regiao"] = a.players
    if a.weeks:
        p["semanas"] = a.weeks
    for s in a.set:
        k, v = s.split("=", 1)
        set_path(p, k, v)
    prices = load(os.path.join(ROOT, "tools", "precos_cidade.json"))
    ref = load(os.path.join(ROOT, "tools", "precos_referencia.json"))
    weeks, cities, minted, burned = simulate(p, prices, ref)

    P = p["jogadores_por_regiao"]
    print(f"{p['cidades']} towns, {P} players per region, {p['semanas']} weeks")
    print("\n  week  income/player  " + "  ".join(f"{c['g']['nome']:>22}" for c in cities))
    for row in weeks:
        cells = []
        for x in row["towns"]:
            cells.append(f"{x['tier'][:9]:>9} {x['treasury']/1000:7.0f}k lv{x['mean_level']:.1f}")
        print(f"{row['week']:>6}  {row['mean_income']:13.0f}  " + "  ".join(f"{c:>22}" for c in cells))
        if a.detail:
            for x in row["towns"]:
                print(f"        {x['name']:>10}: raw {x['raw_bought']:8.0f} list {x['list']:7.0f} tax {x['tax']:7.0f} "
                      f"weapons {x['weapons']:3d}/{x['attack_strength']:.1f} lost {x['stations_lost']} score {x['score']:7.0f} ratio {x['ratio']:.2f}")
    print("\n  governor    total prize    mean/week  tiers")
    for c in cities:
        fx = {}
        for t in c["tiers"]:
            fx[t] = fx.get(t, 0) + 1
        print(f"{c['g']['nome']:>10}  {c['prizes']:12.0f}  {c['prizes']/p['semanas']:12.0f}  {fx}")
    tot_m = sum(minted.values())
    tot_b = sum(burned.values())
    print("\ncoin created:   " + ", ".join(f"{k} {v/1000:.0f}k" for k, v in minted.items()) + f"  (total {tot_m/1000:.0f}k)")
    print("coin destroyed: " + ", ".join(f"{k} {v/1000:.0f}k" for k, v in burned.items()) + f"  (total {tot_b/1000:.0f}k)")
    print(f"per player per week: created {tot_m/(P*p['cidades']*p['semanas']):.0f}, destroyed {tot_b/(P*p['cidades']*p['semanas']):.0f}")


if __name__ == "__main__":
    main()
