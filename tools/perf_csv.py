#!/usr/bin/env python3
"""Summary of an Unreal CSV profile (csvprofile; NWLoadTest -nwcsv=): per-frame average of each column, largest first.

  tools/perf_csv.py [file.csv] [--top 30] [--filter Exclusive]
Without a file: the newest in unreal/NWPort/Saved/Profiling/CSV.
"""
import argparse
import csv
import glob
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file", nargs="?")
    ap.add_argument("--top", type=int, default=30)
    ap.add_argument("--filter", default="")
    ap.add_argument("--filtro", dest="filter", help=argparse.SUPPRESS)  # old name, still accepted
    a = ap.parse_args()
    path = a.file or max(glob.glob(os.path.join(ROOT, "unreal/NWPort/Saved/Profiling/CSV/*.csv")), key=os.path.getmtime)
    rows = list(csv.reader(open(path, encoding="utf-8", errors="replace")))
    head = rows[0]
    def numeric(x):
        try:
            float(x)
            return True
        except ValueError:
            return False
    # Frame rows: same number of columns and the 2nd is a number (the 1st is EVENTS, almost always empty).
    data = [r for r in rows[1:] if len(r) == len(head) and numeric(r[1])]
    sums = {}
    for r in data:
        for k, v in zip(head, r):
            try:
                sums[k] = sums.get(k, 0.0) + float(v)
            except ValueError:
                pass
    n = max(1, len(data))
    items = sorted(((v / n, k) for k, v in sums.items() if a.filter.lower() in k.lower()), reverse=True)
    print(f"{path}: {len(data)} frames")
    for v, k in items[: a.top]:
        print(f"{v:10.3f}  {k}")


if __name__ == "__main__":
    main()
