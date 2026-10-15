#!/usr/bin/env python3
"""Simulador da economia das cidades (docs/game/economy.md), semana a semana.

    python3 tools/sim_economia.py [--param tools/economia.json] [--jogadores N] [--semanas N] [--set a.b=valor ...]

Modelo agregado por cidade (uma cidade por região): jogadores fazem dungeons, coletam, vendem matéria-prima à cidade,
compram a lista (poções, catalisadores) e pagam imposto ao fabricar. O governador (estilo em "governadores") escolhe
onde fica o preço na faixa (0 = mínimo, 1 = máximo), imposto, buffs e quanto gasta em mesas e defesa. Ataques a cada
2 dias a partir da semana 2. Saída: moeda criada e destruída, renda do jogador, balancete, faixa e prêmio de cada
governador. Faixas de preço de tools/precos_cidade.json; o resto de tools/economia.json.
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
    """Preço (moedas) na posição pos (0..1) da faixa."""
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
            # --- jogadores: dungeon e brinde (moeda nova)
            dung = P * dungeons * J["moedas_por_dungeon"]
            brinde = P * J["brinde_moedas_semana"]
            minted["dungeon"] += dung
            minted["brinde"] += brinde
            # --- preço médio de compra de matéria-prima na posição do governador
            buy_price = sum(price_at(r, g["preco_compra"]) for r in raw.values()) / len(raw)
            ref_raw = sum(price_at(r, 0.5) for r in raw.values()) / len(raw)
            # Quanto os jogadores querem vender à cidade: sobe com o preço.
            sell_frac = min(1.0, 0.3 + 0.7 * g["preco_compra"])
            supply = P * gather_units * sell_frac
            # Quanto a cidade quer: material do próximo nível de cada mesa (governador ativo) + consertos.
            need = 0.0
            for lv in c["niveis"]:
                if lv < C["nivel_max"]:
                    need += C["evoluir"][lv]["materia_prima"] * act
            need += c.get("conserto", 0.0)
            # Manutenção pelo uso: cada fabricação numa mesa evoluída gasta matéria-prima (cresce com os jogadores).
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
            # Parte da compra que foi para conserto não conta na meta.
            raw_counted = raw_cost * (1.0 - min(1.0, repair_need / need)) if need > 0 else 0.0
            c["cofre"] -= raw_cost
            c["estoque"] += units
            # Paga a manutenção; sem material, uma mesa perde nível.
            if c["estoque"] >= upkeep:
                c["estoque"] -= upkeep
            elif upkeep > 0:
                c["estoque"] = 0.0
                j = max(range(len(c["niveis"])), key=lambda k: c["niveis"][k])
                c["niveis"][j] -= 1
            # --- lista: poções e catalisadores, demanda elástica ao preço
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
            # --- imposto: parte da fabricação na cidade (mesas melhores e buffs atraem, imposto alto afasta)
            avg_lv = sum(c["niveis"]) / len(c["niveis"])
            share = min(1.0, 0.6 + 0.1 * avg_lv + 0.05 * g["buffs"]) * max(0.2, 1.0 - (g["imposto"] - C["imposto_padrao"]) * 4)
            tax = P * J["valor_fabricado_semana"] * share * g["imposto"]
            c["cofre"] += tax
            # --- gastos do governador: buffs, defesa, evoluir
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
                    # Só conta na meta o nível alcançado pela primeira vez (reconstruir depois de ataque não conta).
                    if c["niveis"][i] > c["maximo"][i]:
                        c["maximo"][i] = c["niveis"][i]
                        proj += e["moedas"] + e["materia_prima"] * ref_raw
            # --- ataques (a partir da semana 2, ~3,5 por semana)
            lost = 0
            if w >= 2 and act >= 0:
                for _ in range(3 if w % 2 else 4):
                    # Força do ataque varia ±30%: defesa justa às vezes perde.
                    if c["armas"] < strength * rnd.uniform(0.7, 1.3):
                        for _ in range(C["mesas_perdidas_por_ataque"]):
                            j = max(range(len(c["niveis"])), key=lambda k: c["niveis"][k])
                            if c["niveis"][j] > 0:
                                c["niveis"][j] -= 1
                                c["conserto"] += C["evoluir"][c["niveis"][j]]["materia_prima"] * C["conserto_fracao"]
                                lost += 1
                        c["armas"] = max(0, c["armas"] - 1)
            # --- pontuação e renda
            # O que vai para os jogadores (compra de matéria-prima) pesa mais que o que sai deles (lista, imposto).
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
        # --- faixas do prêmio: razão com a média das cidades da semana
        scores = [x["pontos"] for x in row["cidades"]]
        # Média só das cidades que não ficaram no tenebroso (a ausente não infla as outras).
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
    ap.add_argument("--set", action="append", default=[], help="a.b=valor (JSON) sobrescreve um parâmetro")
    ap.add_argument("--detalhe", action="store_true", help="tabela por cidade em cada semana")
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
    print(f"{p['cidades']} cidades, {P} jogadores por região, {p['semanas']} semanas")
    print("\nsemana  renda/jogador  " + "  ".join(f"{c['g']['nome']:>22}" for c in cities))
    for row in weeks:
        cells = []
        for x in row["cidades"]:
            cells.append(f"{x['faixa'][:9]:>9} {x['cofre']/1000:7.0f}k nv{x['nivel_medio']:.1f}")
        print(f"{row['semana']:>6}  {row['renda_media']:13.0f}  " + "  ".join(f"{c:>22}" for c in cells))
        if a.detalhe:
            for x in row["cidades"]:
                print(f"        {x['nome']:>10}: mp {x['mp_comprada']:8.0f} lista {x['lista']:7.0f} imposto {x['imposto']:7.0f} "
                      f"armas {x['armas']:3d}/{x['forca_ataque']:.1f} perdidas {x['mesas_perdidas']} pontos {x['pontos']:7.0f} razão {x['razao']:.2f}")
    print("\ngovernador   prêmio total  média/semana  faixas")
    for c in cities:
        fx = {}
        for t in c["faixas"]:
            fx[t] = fx.get(t, 0) + 1
        print(f"{c['g']['nome']:>10}  {c['premios']:12.0f}  {c['premios']/p['semanas']:12.0f}  {fx}")
    tot_m = sum(minted.values())
    tot_b = sum(burned.values())
    print("\nmoeda criada:   " + ", ".join(f"{k} {v/1000:.0f}k" for k, v in minted.items()) + f"  (total {tot_m/1000:.0f}k)")
    print("moeda destruída: " + ", ".join(f"{k} {v/1000:.0f}k" for k, v in burned.items()) + f"  (total {tot_b/1000:.0f}k)")
    print(f"por jogador por semana: criada {tot_m/(P*p['cidades']*p['semanas']):.0f}, destruída {tot_b/(P*p['cidades']*p['semanas']):.0f}")


if __name__ == "__main__":
    main()
