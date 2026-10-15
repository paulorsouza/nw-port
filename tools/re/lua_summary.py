"""Summary of an interface script decompiled by unluac (extract/lua/**.lua).

For each method of the script (`function Lk_1(...) ... end` followed by `Ln_1.Name = Lk_1`): bus calls
(`XBus.Event.Method` / `XBus.Broadcast.Method`, spread over 3 lines by unluac), calls to the script's own methods
(`A0_2.Method`), strings and numbers.

    python3 -I tools/re/lua_summary.py extract/lua/lyshineui/inventory/newinventory.lua [filter]
"""
import re, sys

src = open(sys.argv[1], encoding="utf-8", errors="replace").read().split("\n")
filt = sys.argv[2].lower() if len(sys.argv) > 2 else None

funcs = []           # (start, end, local_name)
stack = []
for i, line in enumerate(src):
    m = re.match(r"^function (L\d+_1)\(", line)
    if m:
        stack = [(i, m.group(1))]
        continue
    if stack and line == "end":
        s, name = stack.pop()
        funcs.append((s, i, name))

names = {}
for idx, (s, e, loc) in enumerate(funcs):
    for j in range(e + 1, min(e + 4, len(src))):
        m = re.match(r"^L\d+_1\.([A-Za-z_][A-Za-z0-9_]*) = " + re.escape(loc) + "$", src[j])
        if m:
            names[idx] = m.group(1)
            break
        m = re.match(r"^L\d+_1\[(L\d+_1)\] = " + re.escape(loc) + "$", src[j])
        if m:
            for k in range(s - 1, max(s - 6, 0), -1):
                mk = re.match(r"^" + m.group(1) + r' = "([^"]+)"$', src[k])
                if mk:
                    names[idx] = mk.group(1)
                    break
            break

bus_re = re.compile(r"^\s*(L\d+_\d+) = ([A-Z][A-Za-z0-9]*Bus|DynamicBus)$")
for idx, (s, e, loc) in enumerate(funcs):
    name = names.get(idx, loc)
    body = src[s:e + 1]
    text = "\n".join(body)
    if filt and filt not in name.lower() and filt not in text.lower():
        continue
    calls = []
    for k, line in enumerate(body):
        m = bus_re.match(line)
        if m and k + 2 < len(body):
            a = re.match(r"^\s*\S+ = \S+\.(Event|Broadcast|[A-Za-z]+)$", body[k + 1])
            b = re.match(r"^\s*\S+ = \S+\.([A-Za-z0-9_]+)$", body[k + 2])
            if a and b:
                calls.append(f"{m.group(2)}.{a.group(1)}.{b.group(1)}")
    selfcalls = sorted(set(re.findall(r"= A0_\d\.([A-Z][A-Za-z0-9_]*)\b", text)))
    strings = sorted(set(re.findall(r'"([^"\n]{1,80})"', text)))
    nums = sorted(set(x for x in re.findall(r"(?<![\w.])(-?\d+\.\d+|-?\d{2,})(?![\w.])", text)), key=lambda x: float(x))
    print(f"== {name}  (lines {s + 1}-{e + 1})")
    if calls:
        print("   bus:", ", ".join(dict.fromkeys(calls)))
    if selfcalls:
        print("   self:", ", ".join(selfcalls[:30]))
    if strings:
        print("   strings:", " | ".join(strings[:25]))
    if nums:
        print("   numbers:", " ".join(nums[:25]))
