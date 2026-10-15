---
name: nw-re
description: Asks the nw-re agent, in the background, for one specific reverse-engineering answer from NewWorld.exe (a rule, formula or value the extracted data does not give) and applies the answer in the port. Use when a number or behavior of the game has no source in the datatables, .adb, slices or actionlists.
---

# Ask and apply reverse engineering

The repository keeps the method, not the answers. Nothing depends on past answers: ask again when you need a fact
from the executable. The agent keeps a local cache in `re-local/` (not tracked) and uses it when present. Setup of
Ghidra and the tools: `docs/tech/reverse-engineering.md`.

Damage, crit, grit and mitigation are server-side: they are not in the client, do not ask.

## Ask

Run the agent in the background and continue with something else. One question per agent; at most two at a time
(Ghidra is shared). Do not do the reverse engineering yourself.

```
Agent(subagent_type="nw-re", run_in_background=true, prompt="""
<area>-<short subject>
- Question: what the game's code does, in one or two sentences.
- What for: where the port will use it (file, function) and what it needs (value, formula, unit, sign).
- Hints: strings, class names, columns, cvars already seen.
""")
```

## Apply the answer

1. Number: a constant in code with the answer's value, or `FNWRules::Num(TEXT("<area>.<name>"), value)` when it
   must be tunable without recompiling. The default of `Num` is the value itself: the port must work without
   `re-local/`.
2. Rule or formula: implement it where it applies. Evidence H (hypothesis): use only without an alternative and say
   so in the test item.
3. No executable addresses in code, docs or specs.
4. Test item in `docs/tests/<area>.md`; the area spec updated (remove the item from its open questions).
