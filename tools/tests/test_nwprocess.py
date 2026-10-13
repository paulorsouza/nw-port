import contextlib
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nwprocess
import convert_gatherables


def wait_file(path, timeout=5):
    deadline = time.monotonic() + timeout
    while not path.exists():
        if time.monotonic() >= deadline:
            raise AssertionError(f"processo nao criou {path}")
        time.sleep(0.01)


class ProcessTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="nw process testes ")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def test_store_python_is_refused_before_launching(self):
        target = self.root / "must-not-exist"
        store = r"C:\Users\x\AppData\Local\Microsoft\WindowsApps\PythonSoftwareFoundation.Python.3.12_abc\python.exe"
        with patch.object(nwprocess.nwplatform, "WINDOWS", True), patch.object(sys, "executable", store):
            self.assertTrue(nwprocess.nwplatform.store_python())
            with self.assertRaises(nwprocess.ResourceError):
                nwprocess.run(["unused", str(target)], log=self.root / "store.log")
        self.assertFalse(target.exists())
        with patch.object(nwprocess.nwplatform, "WINDOWS", True), \
                patch.object(sys, "executable", r"C:\Users\x\AppData\Local\Programs\Python\Python313\python.exe"):
            self.assertFalse(nwprocess.nwplatform.store_python())

    def test_system_memory_and_disk_read_without_external_tools(self):
        self.assertGreater(nwprocess.available_memory(), 0)
        nwprocess.require_resources([self.root / "not-created"], disk=1)
        with patch.object(nwprocess, "available_memory", return_value=0):
            with self.assertRaises(nwprocess.ResourceError):
                nwprocess.require_resources(memory=1)

    def test_failure_and_dry_run_leave_steps_pending(self):
        directory = self.root / "steps"
        marker = directory / "01.ok"
        runner = nwprocess.Steps(directory)
        runner.run("01", "success", lambda: None)
        self.assertTrue(marker.is_file())
        called = []
        runner.run("01", "already done", lambda: called.append(True))
        self.assertEqual(called, [])
        forced = nwprocess.Steps(directory, only="01")
        def fail():
            raise RuntimeError("synthetic failure")
        with self.assertRaises(RuntimeError):
            forced.run("01", "forced failure", fail)
        self.assertFalse(marker.exists())
        nwprocess.Steps(directory, dry_run=True).run("02", "preview", lambda: None)
        self.assertFalse((directory / "02.ok").exists())
        runner.run("01", "retry", lambda: None)
        self.assertTrue(marker.exists())

    def test_supervisor_preserves_arguments_and_exit_status(self):
        log = self.root / "command.log"
        nwprocess.run([sys.executable, "-c", "import sys; print(sys.argv[1])", "spaces ç"],
                      cwd=self.root, log=log)
        self.assertIn("spaces", log.read_text(errors="replace"))
        with self.assertRaises(subprocess.CalledProcessError) as result:
            nwprocess.run([sys.executable, "-c", "raise SystemExit(7)"], cwd=self.root, log=log)
        self.assertEqual(result.exception.returncode, 7)

    def test_memory_abort_kills_grandchild_without_killing_unrelated_process(self):
        heartbeat = self.root / "heartbeat"
        child = self.root / "child.py"
        child.write_text(
            "from pathlib import Path\nimport time\n"
            f"p=Path({str(heartbeat)!r})\n"
            "while True:\n p.write_text(str(time.time_ns()))\n time.sleep(.02)\n", encoding="utf-8")
        parent = self.root / "parent.py"
        parent.write_text(
            "import subprocess,sys,time\n"
            f"subprocess.Popen([sys.executable, {str(child)!r}])\n"
            "time.sleep(30)\n", encoding="utf-8")
        unrelated = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
        try:
            def memory():
                return 0 if heartbeat.exists() else nwprocess.GIB
            with patch.object(nwprocess, "available_memory", side_effect=memory):
                with self.assertRaises(nwprocess.ResourceError):
                    nwprocess.run([sys.executable, str(parent)], cwd=self.root, min_memory=1,
                                  timeout=5, poll=.02, log=self.root / "tree.log")
            self.assertTrue(heartbeat.exists())
            first = heartbeat.read_text()
            time.sleep(.15)
            self.assertEqual(heartbeat.read_text(), first)
            self.assertIsNone(unrelated.poll())
        finally:
            unrelated.terminate()
            unrelated.wait()

    def test_timeout_and_preview(self):
        target = self.root / "must-not-exist"
        command = [sys.executable, "-c", f"open({str(target)!r}, 'w').close()"]
        nwprocess.run(command, dry_run=True, log=self.root / "no-log")
        self.assertFalse(target.exists())
        self.assertFalse((self.root / "no-log").exists())
        with self.assertRaises(subprocess.TimeoutExpired):
            nwprocess.run([sys.executable, "-c", "import time; time.sleep(30)"],
                          timeout=.1, poll=.02, cwd=self.root, log=self.root / "timeout.log")

    def test_independent_process_waits_for_lock_and_nested_work_does_not_deadlock(self):
        lock = self.root / "shared.lock"
        ready, entered = self.root / "ready", self.root / "entered"
        code = (
            f"import sys; sys.path.insert(0, {str(Path(nwprocess.__file__).parent)!r})\n"
            "import nwprocess\nfrom pathlib import Path\n"
            f"Path({str(ready)!r}).touch()\n"
            f"with nwprocess.heavy_lock(Path({str(lock)!r})):\n Path({str(entered)!r}).touch()\n")
        env = dict(os.environ)
        env.pop("NW_PESADO", None)
        with patch.dict(os.environ, {"NW_PESADO": ""}):
            process = None
            try:
                with nwprocess.heavy_lock(lock):
                    with nwprocess.heavy_lock(lock):
                        self.assertEqual(os.environ["NW_PESADO"], "1")
                    process = subprocess.Popen([sys.executable, "-c", code], env=env,
                                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
                    wait_file(ready)
                    time.sleep(.1)
                    self.assertFalse(entered.exists())
                _, errors = process.communicate(timeout=5)
                self.assertEqual(process.returncode, 0, errors)
                self.assertTrue(entered.exists())
                self.assertEqual(os.environ.get("NW_PESADO"), "")
            finally:
                if process and process.poll() is None:
                    process.kill()
                    process.wait()

    def test_gatherables_reject_collisions_and_path_traversal(self):
        meshes = self.root / "meshes.txt"
        for text in ("../../outside.cgf", "C:/outside.cgf", "a/tree.cgf\nb/tree.cgf", ".", "bad.txt"):
            meshes.write_text(text, encoding="utf-8")
            with self.assertRaises(ValueError):
                convert_gatherables.mesh_paths(meshes)
        meshes.write_text("objects/tree.cgf\nobjects/tree.cgf\n", encoding="utf-8")
        self.assertEqual(convert_gatherables.mesh_paths(meshes), ["objects/tree.cgf"])


if __name__ == "__main__":
    unittest.main()
