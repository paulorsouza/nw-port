import contextlib
import io
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "windows"))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_migration
import extract_audio


class MigrationTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="nw migrated project ")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()
        self.old = "/home/dev/old project/nw-port"
        self.markers = self.root / "extract/.etapas"
        self.markers.mkdir(parents=True)

    def manifest(self, name):
        folder = self.root / "extract/ue" / name
        files = {"glb": folder / "glb/player.glb", "fbx": folder / "fbx/SK.fbx", "texture": folder / "fbx/textures/diff.png"}
        for path in files.values():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"cached asset fixture")
        payload = b'{"asset":{"version":"2.0"}}'
        payload += b" " * (-len(payload) % 4)
        files["glb"].write_bytes(struct.pack("<4sII", b"glTF", 2, 20 + len(payload)) +
                                struct.pack("<II", len(payload), 0x4E4F534A) + payload)
        def old(path):
            return self.old + "/" + path.relative_to(self.root).as_posix()
        data = {"name": name, "glb": {"character": old(files["glb"])},
                "fbx": {"skeletal_mesh": old(files["fbx"]), "anims": {}, "weapons": {}, "root_bone": "Xform"},
                "composites": {"Idle": {"fbx": old(files["fbx"])}},
                "materials": {"skin": {"Base Color": old(files["texture"])}}, "engine": "/Game/NW/Mesh"}
        document = folder / "manifest.json"
        document.write_text(json.dumps(data), encoding="utf-8")
        return document, files

    def test_rebase_backs_up_metadata_and_keeps_assets_and_success_markers(self):
        document, assets = self.manifest("sword_shield")
        self.manifest("lost_sword")
        for marker in ("11.ok", "ue_02.ok"):
            (self.markers / marker).touch()
        original = document.read_bytes()
        asset_bytes = {key: path.read_bytes() for key, path in assets.items()}
        audit = check_migration.Audit(self.root, [self.old]).scan()
        self.assertIn(document, audit.actions)
        self.assertTrue((self.markers / "11.ok").exists())
        self.assertEqual(document.read_bytes(), original)  # Scan never writes.
        backup = audit.apply()
        self.assertEqual((backup / "files" / document.relative_to(self.root)).read_bytes(), original)
        repaired = json.loads(document.read_text(encoding="utf-8"))
        self.assertEqual(repaired["fbx"]["skeletal_mesh"], assets["fbx"].as_posix())
        self.assertEqual(repaired["engine"], "/Game/NW/Mesh")
        self.assertTrue((self.markers / "11.ok").exists())
        self.assertTrue((self.markers / "ue_02.ok").exists())
        for key, path in assets.items():
            self.assertEqual(path.read_bytes(), asset_bytes[key])
        self.assertEqual(check_migration.Audit(self.root, [self.old]).scan().actions, {})

    def test_missing_fbx_invalidates_only_affected_markers(self):
        _, assets = self.manifest("sword_shield")
        self.manifest("lost_sword")
        assets["fbx"].unlink()
        for marker in ("11.ok", "ue_02.ok", "04.ok"):
            (self.markers / marker).touch()
        adb = self.root / "extract/models/sample.adb"
        adb.parent.mkdir(parents=True)
        adb.write_text("<adb/>")
        audit = check_migration.Audit(self.root, [self.old]).scan()
        self.assertEqual(audit.rerun, {"11", "ue_02"})
        audit.apply()
        self.assertFalse((self.markers / "11.ok").exists())
        self.assertFalse((self.markers / "ue_02.ok").exists())
        self.assertTrue((self.markers / "04.ok").exists())

    def test_missing_bank_and_txtp_are_repaired_without_rerendering_ogg(self):
        sound = self.root / "extract/_raw/sounds/wwise"
        sound.mkdir(parents=True)
        (sound / "sample.bnk").write_bytes(b"bank cache")
        txtp = self.root / "extract/_raw/txtp/music/sample.txtp"
        txtp.parent.mkdir(parents=True)
        old_bank = self.old + "/extract/_raw/sounds/wwise/media/sample.bnk"
        txtp.write_text(f'# keep comment {old_bank}\n"{old_bank}" #s1 ##123.wem\n', encoding="utf-8")
        ogg = self.root / "extract/audio/ready.ogg"
        ogg.parent.mkdir(parents=True)
        ogg.write_bytes(b"existing OGG")
        (self.markers / "05.ok").touch()
        audit = check_migration.Audit(self.root, [self.old]).scan()
        audit.apply()
        new_bank = sound / "media/sample.bnk"
        self.assertEqual(new_bank.read_bytes(), b"bank cache")
        repaired = txtp.read_text(encoding="utf-8")
        self.assertIn('# keep comment ' + old_bank, repaired)
        self.assertIn('"' + new_bank.as_posix() + '" #s1 ##123.wem', repaired)
        self.assertEqual(extract_audio.sources_of(txtp), [("123", (new_bank, 1))])
        self.assertEqual(ogg.read_bytes(), b"existing OGG")
        self.assertTrue((self.markers / "05.ok").exists())

    def test_broken_bank_symlink_is_backed_up_as_link(self):
        sound = self.root / "extract/_raw/sounds/wwise"
        (sound / "media").mkdir(parents=True)
        (sound / "sample.bnk").write_bytes(b"bank")
        link = sound / "media/sample.bnk"
        try:
            link.symlink_to("/missing/old/path/sample.bnk")
        except OSError:
            self.skipTest("host has no permission to create the fixture symlink")
        audit = check_migration.Audit(self.root).scan()
        backup = audit.apply()
        self.assertFalse(link.is_symlink())
        self.assertEqual(link.read_bytes(), b"bank")
        preserved = backup / "files" / link.relative_to(self.root)
        self.assertTrue(preserved.is_symlink())
        self.assertEqual(os.readlink(preserved), "/missing/old/path/sample.bnk")

    def test_ambiguous_or_outside_targets_are_not_guessed(self):
        document = self.root / "extract/ue/example/manifest.json"
        document.parent.mkdir(parents=True)
        one, two = document.parent / "same.fbx", self.root / "same.fbx"
        one.write_bytes(b"one")
        two.write_bytes(b"two")
        audit = check_migration.Audit(self.root, [self.old])
        self.assertEqual(audit.candidate("same.fbx", document), (None, "ambiguous"))
        outside = self.root.parent / "outside.fbx"
        self.assertEqual(audit.candidate(self.old + "/extract/../../outside.fbx", document), (None, "missing"))
        self.assertNotIn(outside, audit.actions)

    def test_apply_rejects_metadata_changed_after_scan(self):
        document, _ = self.manifest("sword_shield")
        audit = check_migration.Audit(self.root, [self.old]).scan()
        document.write_text('{"edited": true}')
        with self.assertRaises(RuntimeError):
            audit.apply()
        self.assertEqual(document.read_text(), '{"edited": true}')

    def test_legacy_build_marker_and_missing_output_are_not_trusted(self):
        for marker in ("01.ok", "ue_01.ok"):
            (self.markers / marker).touch()
        audit = check_migration.Audit(self.root).scan()
        self.assertEqual(audit.rerun, {"01", "ue_01"})
        audit.apply()
        self.assertFalse((self.markers / "01.ok").exists())
        self.assertFalse((self.markers / "ue_01.ok").exists())

    def test_cli_read_only_report_and_apply_are_idempotent(self):
        document, _ = self.manifest("sword_shield")
        original = document.read_bytes()
        report = self.root / "reports/migration.json"
        argv = ["check_migration.py", "--root", str(self.root), "--old-root", self.old, "--report", str(report)]
        with patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(check_migration.main(), 2)
        self.assertEqual(document.read_bytes(), original)
        with patch.object(sys, "argv", [*argv, "--apply"]), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(check_migration.main(), 0)
        first = document.read_bytes()
        with patch.object(sys, "argv", [*argv, "--apply"]), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(check_migration.main(), 0)
        self.assertEqual(document.read_bytes(), first)
        self.assertFalse(json.loads(report.read_text(encoding="utf-8"))["changes"])


if __name__ == "__main__":
    unittest.main()
