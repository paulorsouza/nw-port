#!/usr/bin/env python3
"""Extrai o áudio Wwise do New World para .ogg em extract/audio/.

Uso: python3 tools/extract_audio.py [--game DIR] [--jobs N] [--quality Q]

Etapas (cada uma pula o que já existe em extract/_raw/):
  1. nwbt unpack: .bnk/.csv de sounds/wwise/ e os .wem de sounds/wwise/media/.
  2. Lista de nomes: tokens de .adb/.xml/.json/.timeline/... do jogo + CSVs de
     eventos + nomes dos bancos. O wwiser compara o hash FNV-1 de cada token
     com os IDs dos eventos, states e switches.
  3. wwiser gera um .txtp por evento/variação, por grupo de bancos.
  4. vgmstream-cli renderiza cada .txtp (ou .wem) e oggenc grava o .ogg.
  5. Grava INDEX.md, sfx_index.csv e ambient/index.csv. Remove .ogg que não
     estão mais no plano (nomes de execuções anteriores).

Requer bin/nwbt, bin/vgmstream-cli, bin/oggenc e vendor/wwiser (ver README).
"""
import argparse
import csv
import os
import re
import subprocess
import sys
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
DEFAULT_GAME = "/mnt/jogos/Steam/steamapps/common/New World"

# Bancos de (b) combate/movimento. Todos levam -gra (um .txtp por variação aleatória).
SFX_GROUPS = ["wpn_1h_sword", "wpn_1h_shield", "impacts", "player_mvts",
              "playvoice_male", "playvoice_female", "bodyfalls", "global"]

# Eventos de cada grupo que entram em (b), e a pasta de destino.
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
    ("playvoice_male", r"voice=vo_(exert|hurt|death|exhausted|landhard|landsoft|holding)", "combat/voice_male"),
    ("playvoice_female", r"voice=vo_(exert|hurt|death|exhausted|landhard|landsoft|holding)", "combat/voice_female"),
]

# Cues de música acima disso são cortados (sequências que somam horas), com
# fade de 10 s; o conteúdo completo está em music/tracks/.
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
    nwbt = BIN / "nwbt"
    env = dict(os.environ, NWBT_OODLE=str(BIN / "libooz.so"))
    if not any(SND.glob("*.bnk")):
        log("unpack: bancos")
        run([nwbt, "unpack", "-g", game, "-o", RAW, "-e", r"^sounds/wwise/[^/]+\.(bnk|csv)$"], env=env)
    if not MEDIA.is_dir() or not any(MEDIA.glob("*.wem")):
        log("unpack: media")
        run([nwbt, "unpack", "-g", game, "-o", RAW, "-e", r"^sounds/wwise/media/.*\.wem$"], env=env)
    # O .txtp aponta para <wemdir>/<banco>.bnk quando o som está dentro do banco.
    for b in SND.glob("*.bnk"):
        link = MEDIA / b.name
        if not link.exists():
            link.symlink_to(Path("..") / b.name)


# ---------------------------------------------------------------- 2. nomes
def build_names(game):
    if WWNAMES.exists():
        return
    nwbt = BIN / "nwbt"
    env = dict(os.environ, NWBT_OODLE=str(BIN / "libooz.so"))
    if not NAMESRC.is_dir():
        log("names: unpack de arquivos de texto do jogo")
        for rx in [r"\.(adb|xml|json|luac|actionlist|chrparams|cdf|aliasasset|musicsheetc)$",
                   r"^(libs/|timelines/[^v]).*\.timeline$"]:
            run([nwbt, "unpack", "-g", game, "-o", NAMESRC, "-e", rx], env=env)
    log("names: coletando tokens")
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
    # -gra: um .txtp por item de random na base; -gd: lista também os duplicados
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


# ---------------------------------------------------------------- 4. nomes de saída
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
    """Cues cujo último state não muda o áudio: o wwiser grava só o primeiro
    valor (ex. poi_state=abandoned_village) e marca os outros como {d}."""
    sib = defaultdict(lambda: [0, 0])  # chave -> [originais, duplicados]
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
    """[(wem_id, (arquivo, subsong|None))] na ordem do .txtp, sem repetir."""
    out, seen = [], set()
    for line in txtp.read_text(errors="replace").splitlines():
        if line.lstrip().startswith("#"):
            continue
        m = re.search(r"([^\s?]\S*\.bnk) #s(\d+).*?##(\d+)\.wem", line)
        if m:
            wid, src = m.group(3), (Path(m.group(1)), int(m.group(2)))
        else:
            m = re.search(r"([^\s?]\S*/(\d+)\.wem)", line)
            if not m:
                continue
            wid, src = m.group(2), (Path(m.group(1)), None)
        if wid not in seen:
            seen.add(wid)
            out.append((wid, src))
    return out


def plan(sfx_rows=None):
    jobs = []          # ((arquivo, subsong|None), dest, title)
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

    # (a) música: eventos renderizados como tocam no jogo
    music_src = {}
    for t in sorted((TXTP / "music").glob("*.txtp")):
        sub, name = music_dest(t.stem)
        add((t, None), sub, name, t.stem, MAX_CUE_SECONDS)
        for w, src in sources_of(t):
            if w not in music_src or len(t.stem) < len(music_src[w][0].stem):
                music_src[w] = (t, src)
    # (a) música: cada .wem de música uma vez, inclusive as faixas que o
    # .txtp não toca (playlists aleatórias tocam só a primeira)
    for w, (t, src) in sorted(music_src.items()):
        sub, name = music_dest(t.stem)
        sub = sub.replace("music/", "music/tracks/", 1)
        add(src, sub, f"{name}__{w}", f"{t.stem} / {w}")
    # (a) músicas de instrumento (mxmi_*)
    for g in sorted(TXTP.glob("mxmi_*")):
        for t in sorted(g.glob("*.txtp")):
            base, var = readable(t.stem)
            name = clean(base.replace("play_mxmi_", "") + ("_" + var if var else ""))
            add((t, None), f"music/instruments/{g.name.replace('mxmi_', '')}", name, t.stem)
    # (b) combate e movimento: cada som-fonte do evento vira um arquivo
    # (as variações aleatórias e as camadas ficam separadas)
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
                if voice:  # playvoice: um evento só; a pasta vem do switch voice
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
        info = subprocess.run([BIN / "vgmstream-cli", "-m", "-i", src],
                              capture_output=True, text=True).stdout
        n = re.search(r"play duration: (\d+) samples", info)
        r = re.search(r"sample rate: (\d+) Hz", info)
        if n and r and int(n.group(1)) / int(r.group(1)) > max_s:
            # .txtp que toca o original por max_s segundos + fade
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
    vgm = subprocess.Popen([BIN / "vgmstream-cli", "-i", "-p", *sel, src],
                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    enc = subprocess.run([BIN / "oggenc", "-Q", "-q", str(quality), "-t", title,
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


# ---------------------------------------------------------------- 6. índice de ambiente
def ambient_index(jobs):
    """Uma linha por evento + combinação de switch: nº de sons, bancos, duração."""
    def row(t):
        base, var = readable(t.stem)
        srcs = sources_of(t)
        banks = sorted({src[0].stem if src[1] else "media" for _, src in srcs})
        info = subprocess.run([BIN / "vgmstream-cli", "-m", "-i", t], capture_output=True, text=True).stdout
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
    ("music/menu", "tela inicial, FTUE, loading (`play_mx_mainmenu`)"),
    ("music/gameplay", "`play_mx_gameplay` por contexto: `territory/` (tema de cada território e das cidades por mês/evento), `combat/`, `dungeon/`, `arenapve/`, `mount*/`, `outpostrush/`..."),
    ("music/cinematics", "`play_mx_igc`"),
    ("music/darkness", "`play_mx_darkness`"),
    ("music/stingers", "stingers e transições"),
    ("music/other", "outros eventos do banco de música"),
    ("music/tracks", "cada .wem de música uma vez; nome = cue mais curto que usa o arquivo + id"),
    ("music/instruments", "músicas de instrumento (`mxmi_*`); prefixo = instrumento (ag, drm, man, urb, azf)"),
    ("combat/sword", "`wpn_1h_sword`: golpes (`rhsword_atk_*`), habilidades, sacar/guardar"),
    ("combat/shield", "`wpn_1h_shield`: defesa, habilidades, sacar/guardar"),
    ("combat/impacts", "`impacts`: espada e escudo por superfície, bloqueios contra escudo, quebra de escudo"),
    ("combat/whoosh", "golpe desarmado"),
    ("combat/voice_male", "esforço, dor, morte, queda (`playvoice_male`)"),
    ("combat/voice_female", "idem (`playvoice_female`)"),
    ("combat/bodyfall_npc", "`bodyfalls`"),
    ("movement/footsteps", "walk/run/sprint/sprintstop/slide do jogador por superfície"),
    ("movement/dodge", "`dodgeroll_player` por superfície"),
    ("movement/jump_land", "pulo e aterrissagem"),
    ("movement/bodyfall", "queda do corpo e mãos no chão"),
]


def write_index():
    def stats(d):
        files = list((OUT / d).rglob("*.ogg"))
        return len(files), sum(f.stat().st_size for f in files)

    lines = ["# Áudio do New World", "",
             "Gerado por `tools/extract_audio.py`. Ogg Vorbis q5, taxa e canais originais.", "",
             "## Categorias", "", "| Pasta | Arquivos | MB | Conteúdo |", "|---|---:|---:|---|"]
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
        "", f"`ambient/index.csv`: só índice. {n_amb} combinações evento/switch de {n_amb_ev} eventos "
        f"(`amb_*` e `ambiences`), com nº de sons, duração renderizada e bancos ({len(amb_banks)} bancos citados). "
        "O ambiente do mundo é quase todo `play_quadamb_wildlife`/`pois`/`rain`/`shell` com switch por bioma e local.",
        "", "## Como os nomes foram mapeados", "",
        "- Os .wem e os sons dentro dos .bnk têm só ID numérico. Eventos, states e switches do Wwise são hash FNV-1 do nome em minúsculas.",
        "- Nomes candidatos: tokens dos .adb, .xml, .json, .timeline, .luac, .actionlist, .chrparams e .cdf do jogo "
        "(ex.: `StartTrigger value=\"Play_RHSword_Atk_PrimA1\"` nos .adb), os CSVs `sounds/wwise/*_events.csv` "
        "(nome e ID, só NPCs) e os nomes dos bancos. Lista em `extract/_raw/wwnames.txt`.",
        "- O wwiser lê os bancos, resolve os hashes com essa lista e gera um .txtp por evento e combinação de state/switch. "
        "O vgmstream-cli toca o .txtp ou o som-fonte; o oggenc grava o .ogg.",
        "- Música: cada arquivo é o evento como toca no jogo (camadas, sequência, volumes). O nome são os valores dos states.",
        "- Combate e movimento: cada som-fonte do evento vira um arquivo, com as variações aleatórias e as camadas separadas: "
        "`<categoria>/<evento>/<evento>_<switch>_NN.ogg`. `sfx_index.csv` liga arquivo, evento, .txtp, ID do .wem e banco.",
        "- O título e o comentário `source` de cada .ogg guardam o .txtp ou .wem de origem.",
        "", "## O que ficou de fora", "",
        f"- Ambiente: só o índice.",
        f"- {n_long} cues de música passam de {MAX_CUE_SECONDS // 60} min (sequências longas) e foram cortados "
        f"em {MAX_CUE_SECONDS // 60} min com fade de 10 s. Lista em `extract/_raw/capped_long_cues.txt`. "
        "O áudio completo está em `music/tracks/`.",
        "- Cues com playlist aleatória tocam só a primeira faixa. As outras estão em `music/tracks/`.",
        "- Stingers e alguns states sem nome no jogo ficam com o ID numérico.",
        "- Resto dos bancos de `sounds/wwise/`: NPCs (294 bancos, a maior parte), outras armas, projéteis, montarias, "
        "estruturas, UI, foley de armadura (`fly_*`), coleta, missões.",
        "- Voz localizada (`localization/sounds/wwise/`, ~16,9 mil .wem) e bancos `minspec/`.",
        f"- Falhas de conversão: {n_fail} (`extract/_raw/render_failures.txt`).",
        "",
    ]
    (OUT / "INDEX.md").write_text("\n".join(lines))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", default=os.environ.get("GAME", DEFAULT_GAME))
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    ap.add_argument("--quality", type=float, default=5)
    ap.add_argument("--index-only", action="store_true", help="só reescreve INDEX.md")
    args = ap.parse_args()
    if args.index_only:
        write_index()
        return

    unpack(args.game)
    build_names(args.game)
    groups = txtp_groups()
    log(f"txtp: {len(groups)} grupos")
    with ThreadPoolExecutor(args.jobs) as ex:
        list(ex.map(lambda kv: gen_txtp(kv[0], *kv[1]), groups.items()))

    sfx_rows = []
    jobs = plan(sfx_rows)
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / "sfx_index.csv").open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["file", "event", "txtp", "wem_id", "source"])
        w.writerows(sfx_rows)
    log(f"render: {len(jobs)} arquivos")
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
    for f in orphans:  # saída de execuções anteriores com outro nome
        f.unlink()
    if orphans:
        log(f"removidos {len(orphans)} .ogg que não estão mais no plano")
    n_amb, n_amb_ev = ambient_index(args.jobs)
    (RAW / "render_failures.txt").write_text("".join(f + "\n" for f in fails))
    (RAW / "capped_long_cues.txt").write_text("".join(f + "\n" for f in longs))
    write_index()
    log(f"ok: {len(jobs) - len(fails)}; cues cortados: {len(longs)}; "
        f"falhas: {len(fails)} (extract/_raw/render_failures.txt); "
        f"ambiente: {n_amb} linhas de {n_amb_ev} eventos no índice")


if __name__ == "__main__":
    main()
