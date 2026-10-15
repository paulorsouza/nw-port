#!/usr/bin/env python3
"""Extracts New World's Wwise audio to .ogg in extract/audio/.

Usage: python3 tools/extract_audio.py [--game DIR] [--jobs N] [--quality Q]

Steps (each one skips what already exists in extract/_raw/):
  1. nwbt unpack: .bnk/.csv from sounds/wwise/ and the .wem from sounds/wwise/media/.
  2. Name list: tokens from the game's .adb/.xml/.json/.timeline/... + event
     CSVs + bank names. wwiser compares the FNV-1 hash of each token
     with the IDs of events, states and switches.
  3. wwiser writes one .txtp per event/variation, per bank group.
  4. vgmstream-cli renders each .txtp (or .wem) and oggenc writes the .ogg.
  5. Writes INDEX.md, sfx_index.csv and ambient/index.csv. Removes .ogg files that
     are no longer in the plan (names from previous runs).

Requires nwbt, vgmstream-cli, oggenc and vendor/wwiser (tools/linux/get_tools.sh, skill nw-setup).
"""
import argparse
import csv
import os
import re
import subprocess
import sys
import shutil
import nwplatform
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "bin"
RAW = ROOT / "extract" / "_raw"
SND = RAW / "sounds" / "wwise"
MEDIA = SND / "media"
NAMESRC = RAW / "names"
WWNAMES = RAW / "wwnames.txt"
TXTP = RAW / "txtp"
OUT = ROOT / "extract" / "audio"
WWISER = ROOT / "vendor" / "wwiser" / "wwiser.py"
DEFAULT_GAME = nwplatform.game_directory()

# Banks for (b) combat/movement. All get -gra (one .txtp per random variation).
SFX_GROUPS = ["wpn_1h_sword", "wpn_1h_shield", "impacts", "player_mvts",
              "playvoice_male", "playvoice_female", "bodyfalls", "global",
              "npc_boar", "npc_wolf", "wpn_1h_rapier", "wpn_2h_bow", "wpn_1h_axe", "ui"]

# Events of each group that go into (b), and the destination folder.
SFX_RULES = [
    ("wpn_1h_sword", r".", "combat/sword"),
    ("wpn_1h_shield", r".", "combat/shield"),
    ("impacts", r"^play_(imp_sword|imp_shield|block_sword|block_shield|block_wpn_.*_vs_shield|shield_break)",
     "combat/impacts"),
    ("global", r"^play_gpui_blockbreaker", "combat/impacts"),
    ("player_mvts", r"^(walk|run|sprint|sprintstop|slide)_player", "movement/footsteps"),
    ("player_mvts", r"^(dodgeroll)_player", "movement/dodge"),
    ("player_mvts", r"^(jump|landhard|landsoft)_player|^play_inair_fall", "movement/jump_land"),
    ("player_mvts", r"^(bodyfall|handhard|handsoft)_player", "movement/bodyfall"),
    ("player_mvts", r"^play_unarmed_whoosh", "combat/whoosh"),
    ("bodyfalls", r".", "combat/bodyfall_npc"),
    # Creatures (G3): voice, pain, death and attacks of each animal (npc_<animal>_events.csv lists the events).
    ("npc_boar", r".", "creatures/boar"),
    ("npc_wolf", r".", "creatures/wolf"),
    # Player weapons besides the sword (Rapier, Bow, Hatchet): attacks, abilities, bowstring, arrow.
    ("wpn_1h_rapier", r".", "combat/rapier"),
    ("wpn_2h_bow", r".", "combat/bow"),
    ("wpn_1h_axe", r".", "combat/hatchet"),
    # Crafting station UI (area 4): craft roll, rarity, perks, gem, trade skill level.
    ("ui", r"^play_ui_(crafting_|tradeskill_levelup)", "ui/crafting"),
    ("playvoice_male", r"voice=vo_(exert|hurt|death|exhausted|landhard|landsoft|holding)", "combat/voice_male"),
    ("playvoice_female", r"voice=vo_(exert|hurt|death|exhausted|landhard|landsoft|holding)", "combat/voice_female"),
]

# Music cues above this are cut (sequences that add up to hours), with
# a 10 s fade; the full content is in music/tracks/.
MAX_CUE_SECONDS = 900

MUSIC_SUB = {
    "play_mx_mainmenu": "menu",
    "play_mx_igc": "cinematics",
    "play_mx_darkness": "darkness",
}


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def log(*a):
    print(*a, flush=True)


# ---------------------------------------------------------------- 1. unpack
def unpack(game):
    nwbt = nwplatform.executable("nwbt")
    env = nwplatform.environment()
    if not any(SND.glob("*.bnk")):
        log("unpack: banks")
        run([nwbt, "unpack", "-g", game, "-o", RAW, "-e", r"^sounds/wwise/[^/]+\.(bnk|csv)$"], env=env)
    if not MEDIA.is_dir() or not any(MEDIA.glob("*.wem")):
        log("unpack: media")
        run([nwbt, "unpack", "-g", game, "-o", RAW, "-e", r"^sounds/wwise/media/.*\.wem$"], env=env)
    # The .txtp points to <wemdir>/<bank>.bnk when the sound is inside the bank.
    for b in SND.glob("*.bnk"):
        link = MEDIA / b.name
        if not link.exists():
            if nwplatform.WINDOWS:
                # Avoids requiring developer mode or the symlink privilege.
                shutil.copy2(b, link)
            else:
                link.symlink_to(Path("..") / b.name)


# ---------------------------------------------------------------- 2. names
def build_names(game):
    if WWNAMES.exists():
        return
    nwbt = nwplatform.executable("nwbt")
    env = nwplatform.environment()
    if not NAMESRC.is_dir():
        log("names: unpacking the game's text files")
        for rx in [r"\.(adb|xml|json|luac|actionlist|chrparams|cdf|aliasasset|musicsheetc)$",
                   r"^(libs/|timelines/[^v]).*\.timeline$"]:
            run([nwbt, "unpack", "-g", game, "-o", NAMESRC, "-e", rx], env=env)
    log("names: collecting tokens")
    tok = re.compile(rb"[A-Za-z][A-Za-z0-9_]{2,80}")
    names = set()
    for dirpath, _, files in os.walk(NAMESRC):
        for f in files:
            try:
                data = (Path(dirpath) / f).read_bytes()
            except OSError:
                continue
            names.update(m.lower() for m in tok.findall(data))
    for c in SND.glob("*_events.csv"):
        for row in csv.reader(c.open(encoding="utf-8", errors="replace")):
            if row and row[0] != "Name":
                names.add(row[0].strip().lower().encode())
    for b in SND.glob("*.bnk"):
        names.add(b.stem.lower().encode())
    WWNAMES.write_bytes(b"\n".join(sorted(names)) + b"\n")
    log(f"names: {len(names)} tokens")


# ---------------------------------------------------------------- 3. txtp
def bank_group(stem):
    banks = [SND / f"{stem}.bnk"]
    ev = SND / f"{stem}_events.bnk"
    if ev.exists():
        banks.append(ev)
    return banks


def txtp_groups():
    # -gra: one .txtp per random item at the base; -gd: also lists the duplicates
    groups = {"music": (bank_group("music"), ["-gra"]),
              "music_dupes": (bank_group("music"), ["-gra", "-gd"])}
    for b in sorted(SND.glob("mxmi_*.bnk")):
        if not b.stem.endswith("_events"):
            groups[b.stem] = (bank_group(b.stem), ["-gra"])
    for g in SFX_GROUPS:
        groups[g] = (bank_group(g), ["-gra"])
    amb = sorted(p for p in SND.glob("amb*.bnk"))
    groups["ambient"] = (amb, [])
    return groups


def gen_txtp(name, banks, flags):
    out = TXTP / name
    if out.is_dir() and any(out.glob("*.txtp")):
        return
    cmd = [sys.executable, WWISER, "-nl", WWNAMES, SND / "init.bnk", *banks,
           "-g", "-go", f"{out}/", "-gw", str(MEDIA), *flags]
    run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


# ---------------------------------------------------------------- 4. output names
def readable(stem):
    """'run_player [surfacetype=grass] {r3}' -> ('run_player', 'grass_r3')."""
    stinger = re.match(r"^(.*?)~\{(stinger|transition)-\d+\}(?:=(\S+))?(.*)$", stem)
    if stinger:
        base, kind, val, rest = stinger.groups()
        parts = [kind] + ([val] if val else [])
        stem = base + rest
    else:
        parts = []
    base = stem.split(" ", 1)[0]
    for m in re.finditer(r"[\(\[]([^=\)\]]+)=([^\)\]]+)[\)\]]|\{(r\d+)\}", stem):
        parts.append(m.group(2) or m.group(3))
    return base, "_".join(parts)


def clean(s):
    s = re.sub(r"[^a-z0-9_\-]+", "_", s.lower())
    return re.sub(r"_+", "_", s).strip("_")


_LAST_STATE = re.compile(r" \(([^=)]+)=([^)]+)\)(?!.*\()")


def generic_states():
    """Cues whose last state does not change the audio: wwiser writes only the first
    value (e.g. poi_state=abandoned_village) and marks the others as {d}."""
    sib = defaultdict(lambda: [0, 0])  # key -> [originals, duplicates]
    for t in (TXTP / "music_dupes").glob("*.txtp"):
        dupe = " {d}" in t.stem
        stem = t.stem.replace(" {d}", "")
        m = _LAST_STATE.search(stem)
        if m:
            key = stem[:m.start()] + f" ({m.group(1)}=*)" + stem[m.end():]
            sib[key][1 if dupe else 0] += 1
    out = set()
    for t in (TXTP / "music").glob("*.txtp"):
        m = _LAST_STATE.search(t.stem)
        if m:
            key = t.stem[:m.start()] + f" ({m.group(1)}=*)" + t.stem[m.end():]
            orig, dup = sib.get(key, (0, 0))
            if orig == 1 and dup >= 2:
                out.add(t.stem)
    return out


GENERIC = None


def music_dest(stem):
    global GENERIC
    if GENERIC is None:
        GENERIC = generic_states()
    if stem in GENERIC:
        stem = _LAST_STATE.sub("", stem, count=1)
    base, var = readable(stem)
    if "~{" in stem:
        sub = "music/stingers"
        return sub, clean(f"{base.replace('play_mx_', '')}_{var}")
    if base == "play_mx_gameplay":
        m = re.search(r"\(mx_context=([^)]+)\)", stem)
        ctx = m.group(1) if m else "other"
        var = re.sub(rf"^{re.escape(ctx)}_?", "", var)
        return f"music/gameplay/{clean(ctx)}", clean(var) or "main"
    sub = MUSIC_SUB.get(base, "other")
    name = var if sub != "other" else f"{base}_{var}"
    return f"music/{sub}", clean(name) or "main"


def sources_of(txtp):
    """[(wem_id, (file, subsong|None))] in .txtp order, without repeats."""
    out, seen = [], set()
    for line in txtp.read_text(errors="replace").splitlines():
        if line.lstrip().startswith("#"):
            continue
        m = re.search(r'^\s*(?!\?)([^#\r\n]+?\.bnk)"?\s+#s(\d+).*?##(\d+)\.wem', line)
        if m:
            wid, src = m.group(3), (Path(m.group(1).strip().strip('"')), int(m.group(2)))
        else:
            m = re.search(r'^\s*(?!\?)([^#\r\n]+?[/\\](\d+)\.wem)', line)
            if not m:
                continue
            wid, src = m.group(2), (Path(m.group(1).strip().strip('"')), None)
        if wid not in seen:
            seen.add(wid)
            out.append((wid, src))
    return out


def plan(sfx_rows=None):
    jobs = []          # ((file, subsong|None), dest, title)
    taken = set()
    if sfx_rows is None:
        sfx_rows = []

    def add(src, sub, name, title, max_s=None):
        dest = OUT / sub / f"{name}.ogg"
        n = 2
        while dest in taken:
            dest = OUT / sub / f"{name}_{n}.ogg"
            n += 1
        taken.add(dest)
        jobs.append((src, dest, title, max_s))
        return dest

    # (a) music: events rendered as they play in the game
    music_src = {}
    for t in sorted((TXTP / "music").glob("*.txtp")):
        sub, name = music_dest(t.stem)
        add((t, None), sub, name, t.stem, MAX_CUE_SECONDS)
        for w, src in sources_of(t):
            if w not in music_src or len(t.stem) < len(music_src[w][0].stem):
                music_src[w] = (t, src)
    # (a) music: each music .wem once, including the tracks the
    # .txtp does not play (random playlists play only the first)
    for w, (t, src) in sorted(music_src.items()):
        sub, name = music_dest(t.stem)
        sub = sub.replace("music/", "music/tracks/", 1)
        add(src, sub, f"{name}__{w}", f"{t.stem} / {w}")
    # (a) instrument music (mxmi_*)
    for g in sorted(TXTP.glob("mxmi_*")):
        for t in sorted(g.glob("*.txtp")):
            base, var = readable(t.stem)
            name = clean(base.replace("play_mxmi_", "") + ("_" + var if var else ""))
            add((t, None), f"music/instruments/{g.name.replace('mxmi_', '')}", name, t.stem)
    # (b) combat and movement: each source sound of the event becomes a file
    # (random variations and layers stay separate)
    for group, rx, sub in SFX_RULES:
        events = defaultdict(list)
        for t in sorted((TXTP / group).glob("*.txtp")):
            if re.search(rx, t.stem):
                events[readable(t.stem)[0]].append(t)
        for base, txtps in sorted(events.items()):
            seen, count = set(), defaultdict(int)
            for t in txtps:
                ev = clean(re.sub(r"^play_", "", base))
                var = re.sub(r"_?r\d+$", "", readable(t.stem)[1])
                voice = re.search(r"\[voice=([^\]]+)\]", t.stem)
                if voice:  # playvoice: a single event; the folder comes from the voice switch
                    ev = clean(voice.group(1))
                    var = re.sub(rf"^{re.escape(voice.group(1))}_?", "", var)
                for w, src in sources_of(t):
                    if w in seen:
                        continue
                    seen.add(w)
                    stem = clean(f"{ev}_{var}" if var else ev)
                    count[stem] += 1
                    dest = add(src, f"{sub}/{ev}", f"{stem}_{count[stem]:02d}", f"{t.stem} / {w}")
                    sfx_rows.append([dest.relative_to(OUT), base, t.stem, w, src[0].name])
    return jobs


# ---------------------------------------------------------------- 5. render
def render(job, quality):
    (src, subsong), dest, title, max_s = job
    status = "ok"
    if max_s:
        info = subprocess.run([nwplatform.executable("vgmstream-cli"), "-m", "-i", src],
                              capture_output=True, text=True).stdout
        n = re.search(r"play duration: (\d+) samples", info)
        r = re.search(r"sample rate: (\d+) Hz", info)
        if n and r and int(n.group(1)) / int(r.group(1)) > max_s:
            # .txtp that plays the original for max_s seconds + fade
            wrap = TXTP / "_capped" / src.name
            wrap.parent.mkdir(parents=True, exist_ok=True)
            wrap.write_text(f"{src} #b {max_s}.0 #f 10.0\n")
            status = f"long {src}"
            src = wrap
    if dest.exists() and dest.stat().st_size > 0:
        return status if status != "ok" else "skip"
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(".part")
    sel = ["-s", str(subsong)] if subsong else []
    vgm = subprocess.Popen([nwplatform.executable("vgmstream-cli"), "-i", "-p", *sel, src],
                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    enc = subprocess.run([nwplatform.executable("oggenc"), "-Q", "-q", str(quality), "-t", title,
                          "-c", f"source={src.name}" + (f"#{subsong}" if subsong else ""),
                          "-o", tmp, "-"],
                         stdin=vgm.stdout, stderr=subprocess.PIPE)
    vgm.stdout.close()
    vgm.wait()
    if vgm.returncode or enc.returncode or not tmp.exists() or tmp.stat().st_size == 0:
        tmp.unlink(missing_ok=True)
        return f"fail {src} {subsong or ''} -> {dest}"
    tmp.rename(dest)
    return status


# ---------------------------------------------------------------- 6. ambient index
def ambient_index(jobs):
    """One row per event + switch combination: number of sounds, banks, duration."""
    def row(t):
        base, var = readable(t.stem)
        srcs = sources_of(t)
        banks = sorted({src[0].stem if src[1] else "media" for _, src in srcs})
        info = subprocess.run([nwplatform.executable("vgmstream-cli"), "-m", "-i", t], capture_output=True, text=True).stdout
        n = re.search(r"play duration: (\d+) samples", info)
        r = re.search(r"sample rate: (\d+) Hz", info)
        dur = f"{int(n.group(1)) / int(r.group(1)):.1f}" if n and r else ""
        return [base, var, len(srcs), dur, " ".join(banks), t.name]

    txtps = sorted((TXTP / "ambient").glob("*.txtp"))
    with ThreadPoolExecutor(jobs) as ex:
        rows = list(ex.map(row, txtps))
    dest = OUT / "ambient" / "index.csv"
    dest.parent.mkdir(parents=True, exist_ok=True)
    with dest.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["event", "switches", "sources", "seconds", "banks", "txtp"])
        w.writerows(rows)
    return len(rows), len({r[0] for r in rows})

# ---------------------------------------------------------------- 7. INDEX.md
CATEGORIES = [
    ("music/menu", "title screen, FTUE, loading (`play_mx_mainmenu`)"),
    ("music/gameplay", "`play_mx_gameplay` by context: `territory/` (theme of each territory and of the towns by month/event), `combat/`, `dungeon/`, `arenapve/`, `mount*/`, `outpostrush/`..."),
    ("music/cinematics", "`play_mx_igc`"),
    ("music/darkness", "`play_mx_darkness`"),
    ("music/stingers", "stingers and transitions"),
    ("music/other", "other events of the music bank"),
    ("music/tracks", "each music .wem once; name = shortest cue that uses the file + id"),
    ("music/instruments", "instrument music (`mxmi_*`); prefix = instrument (ag, drm, man, urb, azf)"),
    ("combat/sword", "`wpn_1h_sword`: attacks (`rhsword_atk_*`), abilities, draw/sheathe"),
    ("combat/shield", "`wpn_1h_shield`: block, abilities, draw/sheathe"),
    ("combat/impacts", "`impacts`: sword and shield by surface, blocks against shield, shield break"),
    ("combat/whoosh", "unarmed attack"),
    ("combat/voice_male", "exertion, pain, death, fall (`playvoice_male`)"),
    ("combat/voice_female", "same (`playvoice_female`)"),
    ("combat/bodyfall_npc", "`bodyfalls`"),
    ("creatures/boar", "`npc_boar`: voice, pain, death, roar, charge, whoosh"),
    ("creatures/wolf", "`npc_wolf`: voice, bite, howl, growl, pain, death, jump"),
    ("combat/rapier", "`wpn_1h_rapier`: Rapier attacks and abilities"),
    ("combat/bow", "`wpn_2h_bow`: bowstring, shot, arrow, Bow abilities"),
    ("combat/hatchet", "`wpn_1h_axe`: Hatchet attacks and throws"),
    ("movement/footsteps", "player walk/run/sprint/sprintstop/slide by surface"),
    ("movement/dodge", "`dodgeroll_player` by surface"),
    ("movement/jump_land", "jump and landing"),
    ("movement/bodyfall", "body fall and hands on the ground"),
]


def write_index():
    def stats(d):
        files = list((OUT / d).rglob("*.ogg"))
        return len(files), sum(f.stat().st_size for f in files)

    lines = ["# New World audio", "",
             "Generated by `tools/extract_audio.py`. Ogg Vorbis q5, original sample rate and channels.", "",
             "## Categories", "", "| Folder | Files | MB | Contents |", "|---|---:|---:|---|"]
    tot_n = tot_b = 0
    for d, desc in CATEGORIES:
        n, b = stats(d)
        tot_n += n
        tot_b += b
        lines.append(f"| `{d}/` | {n} | {b / 1e6:.0f} | {desc} |")
    lines.append(f"| total | {tot_n} | {tot_b / 1e6:.0f} | |")
    amb = OUT / "ambient" / "index.csv"
    amb_rows = list(csv.reader(amb.open()))[1:] if amb.exists() else []
    n_amb, n_amb_ev = len(amb_rows), len({r[0] for r in amb_rows})
    amb_banks = {b for r in amb_rows for b in r[4].split()}
    longs = RAW / "capped_long_cues.txt"
    n_long = len(longs.read_text().splitlines()) if longs.exists() else 0
    fails = RAW / "render_failures.txt"
    n_fail = len(fails.read_text().splitlines()) if fails.exists() else 0
    lines += [
        "", f"`ambient/index.csv`: index only. {n_amb} event/switch combinations of {n_amb_ev} events "
        f"(`amb_*` and `ambiences`), with number of sounds, rendered duration and banks ({len(amb_banks)} banks cited). "
        "The world ambience is almost all `play_quadamb_wildlife`/`pois`/`rain`/`shell` with a switch per biome and place.",
        "", "## How the names were mapped", "",
        "- The .wem files and the sounds inside the .bnk have only a numeric ID. Wwise events, states and switches are the FNV-1 hash of the lowercase name.",
        "- Candidate names: tokens from the game's .adb, .xml, .json, .timeline, .luac, .actionlist, .chrparams and .cdf "
        "(e.g. `StartTrigger value=\"Play_RHSword_Atk_PrimA1\"` in the .adb), the CSVs `sounds/wwise/*_events.csv` "
        "(name and ID, NPCs only) and the bank names. List in `extract/_raw/wwnames.txt`.",
        "- wwiser reads the banks, resolves the hashes with that list and writes one .txtp per event and state/switch combination. "
        "vgmstream-cli plays the .txtp or the source sound; oggenc writes the .ogg.",
        "- Music: each file is the event as it plays in the game (layers, sequence, volumes). The name is the state values.",
        "- Combat and movement: each source sound of the event becomes a file, with random variations and layers separate: "
        "`<category>/<event>/<event>_<switch>_NN.ogg`. `sfx_index.csv` links file, event, .txtp, .wem ID and bank.",
        "- The title and the `source` comment of each .ogg hold the source .txtp or .wem.",
        "", "## What was left out", "",
        f"- Ambience: index only.",
        f"- {n_long} music cues exceed {MAX_CUE_SECONDS // 60} min (long sequences) and were cut "
        f"at {MAX_CUE_SECONDS // 60} min with a 10 s fade. List in `extract/_raw/capped_long_cues.txt`. "
        "The full audio is in `music/tracks/`.",
        "- Cues with a random playlist play only the first track. The others are in `music/tracks/`.",
        "- Stingers and some states without a name in the game keep the numeric ID.",
        "- Rest of the `sounds/wwise/` banks: NPCs (294 banks, most of them), other weapons, projectiles, mounts, "
        "structures, UI, armor foley (`fly_*`), gathering, quests.",
        "- Localized voice (`localization/sounds/wwise/`, ~16.9k .wem) and `minspec/` banks.",
        f"- Conversion failures: {n_fail} (`extract/_raw/render_failures.txt`).",
        "",
    ]
    (OUT / "INDEX.md").write_text("\n".join(lines))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", default=DEFAULT_GAME)
    # Each wwiser reaches ~1.2 GiB; NW_AUDIO_JOBS limits parallelism on machines with little RAM.
    ap.add_argument("--jobs", type=int, default=int(os.environ.get("NW_AUDIO_JOBS") or os.cpu_count()))
    ap.add_argument("--quality", type=float, default=5)
    ap.add_argument("--index-only", action="store_true", help="only rewrites INDEX.md")
    args = ap.parse_args()
    if args.index_only:
        write_index()
        return

    unpack(args.game)
    build_names(args.game)
    groups = txtp_groups()
    log(f"txtp: {len(groups)} groups")
    with ThreadPoolExecutor(args.jobs) as ex:
        list(ex.map(lambda kv: gen_txtp(kv[0], *kv[1]), groups.items()))

    sfx_rows = []
    jobs = plan(sfx_rows)
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / "sfx_index.csv").open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["file", "event", "txtp", "wem_id", "source"])
        w.writerows(sfx_rows)
    log(f"render: {len(jobs)} files")
    fails, longs = [], []
    done = 0
    with ThreadPoolExecutor(args.jobs) as ex:
        for res in ex.map(lambda j: render(j, args.quality), jobs):
            done += 1
            if res.startswith("fail"):
                fails.append(res[5:])
            elif res.startswith("long"):
                longs.append(res[5:])
            if done % 500 == 0:
                log(f"  {done}/{len(jobs)}")
    planned = {j[1] for j in jobs}
    orphans = [f for f in OUT.rglob("*.ogg") if f not in planned]
    for f in orphans:  # output of previous runs with another name
        f.unlink()
    if orphans:
        log(f"removed {len(orphans)} .ogg no longer in the plan")
    n_amb, n_amb_ev = ambient_index(args.jobs)
    (RAW / "render_failures.txt").write_text("".join(f + "\n" for f in fails))
    (RAW / "capped_long_cues.txt").write_text("".join(f + "\n" for f in longs))
    write_index()
    log(f"ok: {len(jobs) - len(fails)}; cues cut: {len(longs)}; "
        f"failures: {len(fails)} (extract/_raw/render_failures.txt); "
        f"ambience: {n_amb} rows of {n_amb_ev} events in the index")


if __name__ == "__main__":
    main()
