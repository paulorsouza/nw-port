"""Contrato dos comandos nativos com fixtures sinteticas, sem assets do jogo."""
import contextlib
import hashlib
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import jogo_versao
import make_data


class NativePipelineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="nw Windows ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "dados sinteticos ç"
        self.game = self.root / "steamapps/common/New World"
        (self.game / "Bin64").mkdir(parents=True)
        payload = b"synthetic executable fixture\x00\xff"
        self.exe = self.game / "Bin64/NewWorld.exe"
        self.exe.write_bytes(payload)
        self.version = self.root / "jogo.versao"
        self.version.write_text(
            "buildid 1234\nexe_sha256 " + hashlib.sha256(payload).hexdigest(), encoding="utf-8")
        self.manifest = self.root / "steamapps/appmanifest_1063730.acf"
        self.manifest.write_text('"AppState"\n{\n "buildid" "1234"\n}', encoding="utf-8")

    def test_build_and_hash_mismatches_block(self):
        self.assertEqual(jogo_versao.check_version(self.game, self.version, True), [])
        self.manifest.write_text('"buildid" "9999"', encoding="utf-8")
        self.assertEqual(len(jogo_versao.check_version(self.game, self.version)), 1)
        self.exe.write_bytes(b"different build")
        self.assertEqual(len(jogo_versao.check_version(self.game, self.version, True)), 2)

    def test_missing_manifest_requires_executable_hash(self):
        self.manifest.unlink()
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(jogo_versao.check_version(self.game, self.version), [])
            self.exe.unlink()
            self.assertEqual(len(jogo_versao.check_version(self.game, self.version)), 1)

    def test_cli_returns_four_unless_explicitly_ignored(self):
        self.exe.write_bytes(b"wrong version")
        self.manifest.unlink()
        with patch.object(jogo_versao.nwplatform, "ROOT", self.root):
            (self.root / "tools").mkdir()
            (self.root / "tools/jogo.versao").write_bytes(self.version.read_bytes())
            with patch.object(sys, "argv", ["jogo_versao.py", "--game", str(self.game)]):
                with patch.dict(os.environ, {"NW_IGNORA_VERSAO": ""}), contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(jogo_versao.main(), 4)
                with patch.dict(os.environ, {"NW_IGNORA_VERSAO": "1"}), contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(jogo_versao.main(), 0)

    def test_data_runner_preserves_paths_cwd_and_stops_on_failure(self):
        script = self.root / "step with spaces.py"
        script.write_text(
            "import pathlib,sys\n"
            "pathlib.Path('executed.txt').write_text(sys.argv[1], encoding='utf-8')\n"
            "raise SystemExit(7)\n", encoding="utf-8")
        last = self.root / "must not execute.py"
        last.write_text("raise RuntimeError('executed after failure')", encoding="utf-8")
        commands = [[str(script), "argumento com espaços ç"], [str(last)]]
        with contextlib.redirect_stdout(io.StringIO()):
            make_data.run_steps(commands, self.root, dry_run=True)
            self.assertFalse((self.root / "executed.txt").exists())
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                make_data.run_steps(commands, self.root)
        self.assertEqual(raised.exception.returncode, 7)
        self.assertEqual((self.root / "executed.txt").read_text(encoding="utf-8"), "argumento com espaços ç")


if __name__ == "__main__":
    unittest.main()
