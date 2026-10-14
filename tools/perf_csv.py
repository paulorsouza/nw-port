#!/usr/bin/env python3
"""Resumo do perfil CSV da Unreal (csvprofile; NWLoadTest -nwcsv=): média por ciclo de cada coluna, maiores primeiro.

  tools/perf_csv.py [arquivo.csv] [--top 30] [--filtro Exclusive]
Sem arquivo: o mais novo de unreal/NWPort/Saved/Profiling/CSV.
"""
import argparse
import csv
import glob
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("arquivo", nargs="?")
    ap.add_argument("--top", type=int, default=30)
    ap.add_argument("--filtro", default="")
    a = ap.parse_args()
    path = a.arquivo or max(glob.glob(os.path.join(ROOT, "unreal/NWPort/Saved/Profiling/CSV/*.csv")), key=os.path.getmtime)
    rows = list(csv.reader(open(path, encoding="utf-8", errors="replace")))
    head = rows[0]
    def numeric(x):
        try:
            float(x)
            return True
        except ValueError:
            return False
    # Linhas de ciclo: mesmo número de colunas e a 2ª é número (a 1ª é EVENTS, vazia quase sempre).
    data = [r for r in rows[1:] if len(r) == len(head) and numeric(r[1])]
    sums = {}
    for r in data:
        for k, v in zip(head, r):
            try:
                sums[k] = sums.get(k, 0.0) + float(v)
            except ValueError:
                pass
    n = max(1, len(data))
    items = sorted(((v / n, k) for k, v in sums.items() if a.filtro.lower() in k.lower()), reverse=True)
    print(f"{path}: {len(data)} ciclos")
    for v, k in items[: a.top]:
        print(f"{v:10.3f}  {k}")


if __name__ == "__main__":
    main()
