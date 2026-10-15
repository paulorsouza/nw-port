"""Renders the .txtp ambiences with native tools, without a shell pipeline."""
import argparse
from pathlib import Path
import re
import subprocess
import sys

import nwplatform
import nwprocess


def sound_name(filename):
    name = filename.removesuffix(".txtp").replace("play_quadamb_", "")
    name = name.replace("(ext_int=amb_ext) ", "")
    name = re.sub(r"\(mx_territory=[a-z]+\) ", "", name)
    name = name.replace("(", "").replace(")", "").replace(" ", "_")
    for prefix in ("tract_state=", "tod_state=", "poi_state="):
        name = name.replace(prefix, "")
    return name


def render(source, destination, decoder=None, encoder=None):
    temporary = destination.with_suffix(".part.ogg")
    destination.parent.mkdir(parents=True, exist_ok=True)
    decoder = decoder or [nwplatform.executable("vgmstream-cli")]
    encoder = encoder or [nwplatform.executable("oggenc")]
    producer = subprocess.Popen([*decoder, "-i", "-p", str(source)], stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL, env=nwplatform.environment())
    try:
        encoded = subprocess.run([*encoder, "-Q", "-q", "4", "-o", str(temporary), "-"],
                                 stdin=producer.stdout, env=nwplatform.environment())
        producer.stdout.close()
        producer.wait()
        if encoded.returncode or producer.returncode or not temporary.is_file() or not temporary.stat().st_size:
            raise RuntimeError(f"render failed: decoder={producer.returncode}, encoder={encoded.returncode}")
        temporary.replace(destination)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise
    finally:
        producer.stdout.close()
        if producer.poll() is None:
            producer.kill()
        producer.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("territory", nargs="?")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--one", nargs=2, type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    try:
        if args.one:
            render(*args.one)
            return 0
        if not args.territory or not re.fullmatch(r"[a-z]+", args.territory):
            parser.error("give the territory, e.g. windsward")
        source_dir = nwplatform.ROOT / "extract/_raw/txtp/ambient"
        output = nwplatform.ROOT / "extract/audio/ambient" / args.territory
        if not source_dir.is_dir():
            raise FileNotFoundError(f"missing {source_dir}; run extract_audio.py")
        matches = sorted(source_dir.glob(f"*mx_territory={args.territory}*.txtp"))
        names = [sound_name(source.name) for source in matches]
        if len({name.casefold() for name in names}) != len(names):
            raise ValueError("audio names collide after conversion")
        if not matches:
            raise RuntimeError(f"no ambience for {args.territory}; step pending")
        with context_lock(args.dry_run):
            for source, name in zip(matches, names):
                destination = output / (name + ".ogg")
                if destination.is_file() and destination.stat().st_size > 0:
                    continue
                nwprocess.run([sys.executable, str(Path(__file__).resolve()), "--one", source, destination],
                              log=output / "render.log", dry_run=args.dry_run)
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(exc, file=sys.stderr)
        return 4
    except KeyboardInterrupt:
        return 130


def context_lock(dry_run):
    from contextlib import nullcontext
    return nullcontext() if dry_run else nwprocess.heavy_lock()


if __name__ == "__main__":
    raise SystemExit(main())
