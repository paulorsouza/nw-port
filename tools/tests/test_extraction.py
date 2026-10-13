import contextlib
import csv
import io
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_all
import convert_city
import convert_gatherables
from nwassets import complete_glb
import nwplatform
import nwprocess
import render_ambient


def write_glb(path):
    model = {"asset": {"version": "2.0"}, "nodes": [{"mesh": 0}], "meshes": [
        {"primitives": [{"material": 0}, {"material": 1}]}], "materials": [
        {"name": "visible", "extensions": {"EXT_nw_mtl": {"attrs": {"Shader": "Illum"}}}},
        {"name": "collision", "extensions": {"EXT_nw_mtl": {"attrs": {}}}}]}
    data = json.dumps(model).encode()
    data += b" " * (-len(data) % 4)
    binary = struct.pack("<II", len(data), 0x4E4F534A) + data + struct.pack("<II", 0, 0x004E4942)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(struct.pack("<4sII", b"glTF", 2, 12 + len(binary)) + binary)


class ExtractionTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="nw fixtures ")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def test_csv_manifest_keeps_spaces_and_filters_icons(self):
        source = self.root / "manifest.csv"
        with source.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.writer(stream, delimiter=";")
            for name in ("objects/path with spaces.cgf", "lyshineui/images/icons/items/sword.dds"):
                writer.writerow([name, "2026-01-01", "10", "01234567"])
        paths, icons = self.root / "paths.txt", self.root / "icons.txt"
        build_all.write_manifest(source, paths, icons)
        self.assertEqual(paths.read_text().splitlines()[0], "objects/path with spaces.cgf")
        self.assertEqual(icons.read_text().splitlines(), ["lyshineui/images/icons/items/sword.dds"])
        source.write_text("", encoding="utf-8")
        previous = paths.read_bytes()
        with self.assertRaises(RuntimeError):
            build_all.write_manifest(source, paths, icons)
        self.assertEqual(paths.read_bytes(), previous)

    def test_full_extraction_preview_neither_launches_nor_writes(self):
        with patch.object(nwplatform, "ROOT", self.root), patch.dict(os.environ, {}):
            with patch.object(sys, "argv", ["build_all.py", "--dry-run"]):
                with patch.object(nwprocess.subprocess, "Popen", side_effect=AssertionError("launched")):
                    with contextlib.redirect_stdout(io.StringIO()) as output:
                        self.assertEqual(build_all.main(), 0)
        self.assertIn("sistemas-03", output.getvalue())
        self.assertIn("[13]", output.getvalue())
        self.assertFalse((self.root / "extract").exists())
        self.assertFalse((self.root / ".nwbt").exists())

    def test_capital_search_handles_region_boundaries_and_duplicates(self):
        path = self.root / "r_+04_+01/capitals/city/city.capitals.json"
        path.parent.mkdir(parents=True)
        capital = {"sliceName": "slices/Settlement_Test_Complete", "worldPosition": {"x": 8192, "y": 2048}}
        path.write_text(json.dumps({"Capitals": [capital]}))
        found, coordinates, count = convert_city.find_capital(self.root, "settlement_test_complete")
        self.assertEqual(found, path)
        self.assertEqual(set(coordinates), {(3, 0), (3, 1), (4, 0), (4, 1)})
        path.write_text(json.dumps({"Capitals": [capital, capital]}))
        self.assertEqual(convert_city.find_capital(self.root, "settlement_test_complete"), (None, [], 2))

    def test_glb_structure_and_real_strip_without_game(self):
        output = self.root / "converted"
        raw = output / "glb/objects/test.cgf.glb"
        write_glb(raw)
        self.assertTrue(complete_glb(raw))
        truncated = self.root / "truncated.glb"
        truncated.write_bytes(raw.read_bytes()[:-4])
        self.assertFalse(complete_glb(truncated))
        listing = self.root / "meshes.txt"
        listing.write_text("objects/test.cgf\n")
        convert_gatherables.convert(listing, output, "no-game-required")
        clean = output / "limpa/test.glb"
        self.assertTrue(complete_glb(clean))
        with clean.open("rb") as stream:
            stream.seek(12)
            size, _ = struct.unpack("<II", stream.read(8))
            model = json.loads(stream.read(size))
        self.assertEqual(len(model["meshes"][0]["primitives"]), 1)

    def test_batch_preview_limits_work_and_does_not_create_output(self):
        listing = self.root / "meshes.txt"
        listing.write_text("\n".join(f"objects/tree{i}.cgf" for i in range(17)))
        with patch.object(nwprocess, "run") as run:
            convert_gatherables.convert(listing, self.root / "output", "fake-game", dry_run=True)
        batches = [call.args[0] for call in run.call_args_list if "--skip" in call.args[0]]
        self.assertEqual(len(batches), 3)
        self.assertTrue(all(len(batch[batch.index("--skip") + 1:]) <= 8 for batch in batches))
        self.assertFalse((self.root / "output").exists())

    def test_audio_promotes_only_when_both_processes_succeed(self):
        decoder, encoder = self.root / "decoder.py", self.root / "encoder.py"
        decoder.write_text("import sys; sys.stdout.buffer.write(b'synthetic WAV')")
        encoder.write_text("import sys,pathlib\np=sys.argv[sys.argv.index('-o')+1]\n"
                           "pathlib.Path(p).write_bytes(b'OGG'+sys.stdin.buffer.read())")
        output = self.root / "ambient.ogg"
        render_ambient.render(self.root / "source.txtp", output,
                              [sys.executable, str(decoder)], [sys.executable, str(encoder)])
        self.assertEqual(output.read_bytes(), b"OGGsynthetic WAV")
        previous = output.read_bytes()
        decoder.write_text("raise SystemExit(7)")
        with self.assertRaises(RuntimeError):
            render_ambient.render(self.root / "source.txtp", output,
                                  [sys.executable, str(decoder)], [sys.executable, str(encoder)])
        self.assertEqual(output.read_bytes(), previous)
        self.assertFalse(output.with_suffix(".part.ogg").exists())


if __name__ == "__main__":
    unittest.main()
