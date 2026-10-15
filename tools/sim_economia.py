#!/usr/bin/env python3
"""Town economy simulator (docs/game/economy.md), week by week.

    python3 tools/sim_economia.py [--param tools/economia.json] [--jogadores N] [--semanas N] [--set a.b=value ...]

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
        cities.append({"g": g, "cofre": C["balancete_inicial"], "estoque": 0.0, "niveis": [0] * C["mesas"], "maximo": [0] * C["mesas"], "armas": 0,
                       "hist": [], "premios": 0, "tenebroso": 0, "faixas": []})
    minted = {"dungeon": 0.0, "brinde": 0.0, "premio": 0.0, "balancete_inicial": C["balancete_inicial"] * len(cities)}
    burned = {"projetos": 0.0, "buffs": 0.0, "armas": 0.0, "compra_cidade": C["preco_cidade"] * len(cities)}
    weeks = []
    rnd = random.Random(p.get("semente", 1))

    hours = J["horas_semana"]
    dungeons = hours * J["fracao_dungeon"] * 60.0 / J["minutos_por_dungeon"]
    gather_units = hours * J["fracao_coleta"] * J["unidades_coleta_hora"]

    for w in range(1, p["semanas"] + 1):
        row = {"semana": w, "cidades": []}
        week_income = []
        for c in cities:
            g = c["g"]
            act = g["atividade"]
            # --- players: dungeon and handout (new coin)
            dung = P * dungeons * J["moedas_por_dungeon"]
            brinde = P * J["brinde_moedas_semana"]
            minted["dungeon"] += dung
            minted["brinde"] += brinde
            # --- mean raw material buy price at the governor's position
            buy_price = sum(price_at(r, g["preco_compra"]) for r in raw.values()) / len(raw)
            ref_raw = sum(price_at(r, 0.5) for r in raw.values()) / len(raw)
            # How much the players want to sell to the town: rises with the price.
            sell_frac = min(1.0, 0.3 + 0.7 * g["preco_compra"])
            supply = P * gather_units * sell_frac
            # How much the town wants: material for each station's next level (active governor) + repairs.
            need = 0.0
            for lv in c["niveis"]:
                if lv < C["nivel_max"]:
                    need += C["evoluir"][lv]["materia_prima"] * act
            need += c.get("conserto", 0.0)
            # Upkeep from use: each craft on an upgraded station spends raw material (grows with the players).
            avg_lv0 = sum(c["niveis"]) / len(c["niveis"])
            use = P * min(1.0, 0.6 + 0.1 * avg_lv0 + 0.05 * g["buffs"])
            upkeep = avg_lv0 * use * C["manutencao_por_jogador_nivel"]
            repair_need = c.get("conserto", 0.0)
            need += upkeep
            need = max(0.0, need - c["estoque"])
            if act == 0:
                need = 0.0
            afford = c["cofre"] / buy_price if buy_price > 0 else 0
            units = min(supply, need, afford, P * C["limite_compra_por_tipo"] * len(raw))
            raw_cost = units * buy_price
            # The part of the purchase that went to repairs does not count toward the goal.
            raw_counted = raw_cost * (1.0 - min(1.0, repair_need / need)) if need > 0 else 0.0
            c["cofre"] -= raw_cost
            c["estoque"] += units
            # Pays the upkeep; without material, a station loses a level.
            if c["estoque"] >= upkeep:
                c["estoque"] -= upkeep
            elif upkeep > 0:
                c["estoque"] = 0.0
                j = max(range(len(c["niveis"])), key=lambda k: c["niveis"][k])
                c["niveis"][j] -= 1
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
            c["cofre"] += list_rev
            # --- tax: share of crafting in the town (better stations and buffs attract, high tax drives away)
            avg_lv = sum(c["niveis"]) / len(c["niveis"])
            share = min(1.0, 0.6 + 0.1 * avg_lv + 0.05 * g["buffs"]) * max(0.2, 1.0 - (g["imposto"] - C["imposto_padrao"]) * 4)
            tax = P * J["valor_fabricado_semana"] * share * g["imposto"]
            c["cofre"] += tax
            # --- governor spending: buffs, defense, upgrades
            buffs = 0
            for _ in range(g["buffs"]):
                if c["cofre"] >= C["buff_moedas"]:
                    c["cofre"] -= C["buff_moedas"]
                    burned["buffs"] += C["buff_moedas"]
                    buffs += 1
            proj = buffs * C["buff_moedas"]
            strength = C["ataque_base"] + C["ataque_por_semana"] * w + C["ataque_por_nivel_cidade"] * sum(c["niveis"])
            if act > 0:
                while c["armas"] < strength * g.get("margem_defesa", 1.0) and c["cofre"] >= C["arma_moedas"]:
                    c["cofre"] -= C["arma_moedas"]
                    burned["armas"] += C["arma_moedas"]
                    proj += C["arma_moedas"]
                    c["armas"] += 1
            c["conserto"] = 0.0
            for i, lv in enumerate(c["niveis"]):
                if lv >= C["nivel_max"] or act == 0:
                    continue
                e = C["evoluir"][lv]
                if c["estoque"] >= e["materia_prima"] and c["cofre"] >= e["moedas"]:
                    c["estoque"] -= e["materia_prima"]
                    c["cofre"] -= e["moedas"]
                    burned["projetos"] += e["moedas"]
                    c["niveis"][i] += 1
                    # Only a level reached for the first time counts toward the goal (rebuilding after an attack does not).
                    if c["niveis"][i] > c["maximo"][i]:
                        c["maximo"][i] = c["niveis"][i]
                        proj += e["moedas"] + e["materia_prima"] * ref_raw
            # --- attacks (from week 2, ~3.5 per week)
            lost = 0
            if w >= 2 and act >= 0:
                for _ in range(3 if w % 2 else 4):
                    # Attack strength varies ±30%: a just-enough defense sometimes loses.
                    if c["armas"] < strength * rnd.uniform(0.7, 1.3):
                        for _ in range(C["mesas_perdidas_por_ataque"]):
                            j = max(range(len(c["niveis"])), key=lambda k: c["niveis"][k])
                            if c["niveis"][j] > 0:
                                c["niveis"][j] -= 1
                                c["conserto"] += C["evoluir"][c["niveis"][j]]["materia_prima"] * C["conserto_fracao"]
                                lost += 1
                        c["armas"] = max(0, c["armas"] - 1)
            # --- score and income
            # What goes to the players (raw material purchases) weighs more than what comes from them (list, tax).
            score = raw_counted + PR["peso_lista"] * list_rev + PR["peso_imposto"] * tax + proj \
                + PR["peso_nivel"] * sum(c["niveis"]) / len(c["niveis"])
            teneb = units < PR["tenebroso_materia_prima"] or list_q < PR["tenebroso_lista"] or buffs == 0
            income = (dung + brinde + raw_cost - list_rev - tax) / P
            week_income.append(income)
            c["hist"].append(score)
            row["cidades"].append({"nome": g["nome"], "cofre": c["cofre"], "estoque": c["estoque"], "mp_comprada": units,
                                   "lista": list_q, "imposto": tax, "nivel_medio": sum(c["niveis"]) / len(c["niveis"]),
                                   "armas": c["armas"], "forca_ataque": strength, "mesas_perdidas": lost,
                                   "pontos": score, "tenebroso": teneb, "renda_jogador": income})
        # --- prize tiers: ratio to the week's town average
        scores = [x["pontos"] for x in row["cidades"]]
        # Average only over towns not in the "tenebroso" tier (an absent one does not inflate the others).
        alive = [x["pontos"] for x in row["cidades"] if not x["tenebroso"]] or scores
        mean = sum(alive) / len(alive) if alive else 1
        best = max(range(len(scores)), key=lambda k: scores[k])
        for k, x in enumerate(row["cidades"]):
            r = x["pontos"] / mean if mean > 0 else 0
            if x["tenebroso"]:
                tier = "tenebroso"
            elif r >= PR["faixas"]["excelente"]:
                tier = "melhor" if k == best else "excelente"
            elif r >= PR["faixas"]["muito_bom"]:
                tier = "muito_bom"
            elif r >= PR["faixas"]["bom"]:
                tier = "bom"
            else:
                tier = "ruim"
            x["razao"] = r
            x["faixa"] = tier
            prize = PR[tier]
            minted["premio"] += prize
            cities[k]["premios"] += prize
            cities[k]["faixas"].append(tier)
        row["renda_media"] = sum(week_income) / len(week_income)
        weeks.append(row)
    return weeks, cities, minted, burned


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--param", default=os.path.join(ROOT, "tools", "economia.json"))
    ap.add_argument("--jogadores", type=int)
    ap.add_argument("--semanas", type=int)
    ap.add_argument("--set", action="append", default=[], help="a.b=value (JSON) overrides a parameter")
    ap.add_argument("--detalhe", action="store_true", help="per-town table for each week")
    a = ap.parse_args()
    p = load(a.param)
    if a.jogadores:
        p["jogadores_por_regiao"] = a.jogadores
    if a.semanas:
        p["semanas"] = a.semanas
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
        for x in row["cidades"]:
            cells.append(f"{x['faixa'][:9]:>9} {x['cofre']/1000:7.0f}k lv{x['nivel_medio']:.1f}")
        print(f"{row['semana']:>6}  {row['renda_media']:13.0f}  " + "  ".join(f"{c:>22}" for c in cells))
        if a.detalhe:
            for x in row["cidades"]:
                print(f"        {x['nome']:>10}: raw {x['mp_comprada']:8.0f} list {x['lista']:7.0f} tax {x['imposto']:7.0f} "
                      f"weapons {x['armas']:3d}/{x['forca_ataque']:.1f} lost {x['mesas_perdidas']} score {x['pontos']:7.0f} ratio {x['razao']:.2f}")
    print("\n  governor    total prize    mean/week  tiers")
    for c in cities:
        fx = {}
        for t in c["faixas"]:
            fx[t] = fx.get(t, 0) + 1
        print(f"{c['g']['nome']:>10}  {c['premios']:12.0f}  {c['premios']/p['semanas']:12.0f}  {fx}")
    tot_m = sum(minted.values())
    tot_b = sum(burned.values())
    print("\ncoin created:   " + ", ".join(f"{k} {v/1000:.0f}k" for k, v in minted.items()) + f"  (total {tot_m/1000:.0f}k)")
    print("coin destroyed: " + ", ".join(f"{k} {v/1000:.0f}k" for k, v in burned.items()) + f"  (total {tot_b/1000:.0f}k)")
    print(f"per player per week: created {tot_m/(P*p['cidades']*p['semanas']):.0f}, destroyed {tot_b/(P*p['cidades']*p['semanas']):.0f}")


if __name__ == "__main__":
    main()
