"""Supervised execution, heavy-job queue and resumable steps on Windows/Linux."""
import contextlib
import ctypes
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time

import nwplatform

GIB = 1024 ** 3


class ResourceError(RuntimeError):
    pass


def available_memory():
    if nwplatform.WINDOWS:
        class MemoryStatus(ctypes.Structure):
            _fields_ = [("length", ctypes.c_uint32), ("load", ctypes.c_uint32)] + [
                (name, ctypes.c_uint64) for name in (
                    "total_phys", "avail_phys", "total_page", "avail_page",
                    "total_virtual", "avail_virtual", "avail_extended")]
        status = MemoryStatus()
        status.length = ctypes.sizeof(status)
        if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
            raise ctypes.WinError()
        return status.avail_phys
    with open("/proc/meminfo", encoding="ascii") as stream:
        for line in stream:
            if line.startswith("MemAvailable:"):
                return int(line.split()[1]) * 1024
    raise ResourceError("MemAvailable unavailable")


def memory_floor(memory):
    # NW_MIN_MEM_GIB caps the memory minimums on machines with little headroom.
    configured = os.environ.get("NW_MIN_MEM_GIB")
    if memory and configured:
        return min(memory, int(float(configured) * GIB))
    return memory


def require_resources(paths=(), memory=0, disk=0):
    memory = memory_floor(memory)
    if memory and available_memory() < memory:
        raise ResourceError(f"less than {memory / GIB:g} GiB of memory available")
    for path in paths:
        existing = Path(path).resolve()
        while not existing.exists() and existing != existing.parent:
            existing = existing.parent
        if disk and shutil.disk_usage(existing).free < disk:
            raise ResourceError(f"less than {disk / GIB:g} GiB free on {existing}")


@contextlib.contextmanager
def heavy_lock(path=None, poll=0.2):
    # Same name/path as pesado.sh on Linux. Children inherit NW_PESADO=1.
    if os.environ.get("NW_PESADO"):
        yield
        return
    path = Path(path or Path(os.environ.get("XDG_RUNTIME_DIR", tempfile.gettempdir())) / "nw-pesado.lock")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b", buffering=0) as stream:
        if nwplatform.WINDOWS:
            import msvcrt
            if path.stat().st_size == 0:
                stream.write(b"\0")
            def acquire():
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            def release():
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl
            def acquire():
                fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            def release():
                fcntl.flock(stream.fileno(), fcntl.LOCK_UN)
        announced = False
        while True:
            try:
                acquire()
                break
            except OSError as exc:
                # A real I/O/permission failure must not turn into an endless wait.
                if exc.errno not in (11, 13, 35, 36):
                    raise
                if not announced:
                    print(f"[pesado] waiting for another heavy job ({path})...", flush=True)
                    announced = True
                time.sleep(poll)
        previous = os.environ.get("NW_PESADO")
        os.environ["NW_PESADO"] = "1"
        try:
            yield
        finally:
            if previous is None:
                os.environ.pop("NW_PESADO", None)
            else:
                os.environ["NW_PESADO"] = previous
            release()


class WindowsJob:
    """Job Object: on close, ends only this job's process tree."""
    def __init__(self):
        from ctypes import wintypes
        class BasicLimits(ctypes.Structure):
            _fields_ = [("process_time", ctypes.c_int64), ("job_time", ctypes.c_int64),
                        ("flags", wintypes.DWORD), ("min_working", ctypes.c_size_t),
                        ("max_working", ctypes.c_size_t), ("active", wintypes.DWORD),
                        ("affinity", ctypes.c_size_t), ("priority", wintypes.DWORD),
                        ("scheduling", wintypes.DWORD)]
        class IoCounters(ctypes.Structure):
            _fields_ = [(name, ctypes.c_uint64) for name in
                        ("read_ops", "write_ops", "other_ops", "read_bytes", "write_bytes", "other_bytes")]
        class ExtendedLimits(ctypes.Structure):
            _fields_ = [("basic", BasicLimits), ("io", IoCounters),
                        ("process_memory", ctypes.c_size_t), ("job_memory", ctypes.c_size_t),
                        ("peak_process_memory", ctypes.c_size_t), ("peak_job_memory", ctypes.c_size_t)]
        self.api = ctypes.WinDLL("kernel32", use_last_error=True)
        self.api.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
        self.api.CreateJobObjectW.restype = wintypes.HANDLE
        self.api.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
        self.api.SetInformationJobObject.restype = wintypes.BOOL
        self.api.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
        self.api.AssignProcessToJobObject.restype = wintypes.BOOL
        self.api.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.api.OpenProcess.restype = wintypes.HANDLE
        self.api.CloseHandle.argtypes = [wintypes.HANDLE]
        self.api.CloseHandle.restype = wintypes.BOOL
        self.api.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
        self.api.TerminateJobObject.restype = wintypes.BOOL
        self.api.QueryInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.c_void_p]
        self.api.QueryInformationJobObject.restype = wintypes.BOOL
        self.handle = self.api.CreateJobObjectW(None, None)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        limits = ExtendedLimits()
        limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if not self.api.SetInformationJobObject(self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
            error = ctypes.get_last_error()
            self.close()
            raise ctypes.WinError(error)

    def assign(self, pid):
        handle = self.api.OpenProcess(0x0101, False, pid)  # SET_QUOTA | TERMINATE
        if not handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            if not self.api.AssignProcessToJobObject(self.handle, handle):
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            self.api.CloseHandle(handle)

    def close(self):
        if self.handle:
            # Job termination is asynchronous. Also wait for the children to release
            # files, even when the parent wrapper has already exited.
            try:
                if not self.api.TerminateJobObject(self.handle, 1):
                    raise ctypes.WinError(ctypes.get_last_error())
                accounting = (ctypes.c_uint64 * 4)()
                counts = (ctypes.c_uint32 * 4)()
                class Accounting(ctypes.Structure):
                    _fields_ = [("times", type(accounting)), ("counts", type(counts))]
                info = Accounting()
                deadline = time.monotonic() + 5
                while True:
                    if not self.api.QueryInformationJobObject(self.handle, 1, ctypes.byref(info), ctypes.sizeof(info), None):
                        raise ctypes.WinError(ctypes.get_last_error())
                    if info.counts[2] == 0:  # ActiveProcesses
                        break
                    if time.monotonic() >= deadline:
                        raise RuntimeError("Windows Job processes did not exit within 5 seconds")
                    time.sleep(.01)
            finally:
                self.api.CloseHandle(self.handle)
                self.handle = None


def stop_group(process):
    # The Linux group remains even if the parent exited leaving grandchildren alive.
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        pass
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def run(command, *, cwd=nwplatform.ROOT, log=None, min_memory=0, timeout=None,
        poll=0.2, dry_run=False):
    command = [str(value) for value in command]
    print("==", subprocess.list2cmdline(command), flush=True)
    if dry_run:
        return
    if nwplatform.store_python():
        raise ResourceError(f"Microsoft Store Python ({sys.executable}) does not contain child processes; "
                            "install Python from python.org and run with it")
    min_memory = memory_floor(min_memory)
    require_resources(memory=min_memory)
    job = None
    process = None
    with contextlib.ExitStack() as stack:
        output = None
        if log:
            log = Path(log)
            log.parent.mkdir(parents=True, exist_ok=True)
            output = stack.enter_context(log.open("ab"))
        kwargs = dict(cwd=cwd, env=nwplatform.environment(), stdout=output,
                      stderr=subprocess.STDOUT if output else None)
        try:
            if nwplatform.WINDOWS:
                job = WindowsJob()
                gate_dir = Path(stack.enter_context(tempfile.TemporaryDirectory(prefix="nw-process-")))
                gate = gate_dir / "ready"
                # The wrapper waits for the Job assignment before creating any child.
                launch = [sys.executable, str(Path(__file__).resolve()), "--child", str(gate), *command]
                process = subprocess.Popen(launch, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS, **kwargs)
                job.assign(process.pid)
                gate.touch()
            else:
                process = subprocess.Popen(command, start_new_session=True, **kwargs)
                try:
                    os.setpriority(os.PRIO_PROCESS, process.pid, 19)
                except ProcessLookupError:
                    pass  # A short command may finish before the priority is set.
            start = time.monotonic()
            while process.poll() is None:
                if min_memory and available_memory() < min_memory:
                    raise ResourceError("memory below the limit during the job")
                if timeout is not None and time.monotonic() - start >= timeout:
                    raise subprocess.TimeoutExpired(command, timeout)
                time.sleep(poll)
            if process.returncode:
                raise subprocess.CalledProcessError(process.returncode, command)
        finally:
            if job:
                job.close()
            elif process and not nwplatform.WINDOWS:
                stop_group(process)
            if process:
                if process.poll() is None and nwplatform.WINDOWS:
                    process.kill()
                process.wait()


class Steps:
    def __init__(self, directory, only=None, dry_run=False, before=None):
        self.directory = Path(directory)
        self.only = only
        self.dry_run = dry_run
        self.before = before

    def run(self, name, description, action):
        if self.only and self.only != name:
            return
        marker = self.directory / f"{name}.ok"
        if not self.only and marker.is_file():
            print(f"[{name}] {description}: already done", flush=True)
            return
        if self.before and not self.dry_run:
            self.before()
        print(f"[{name}] {description}", flush=True)
        # A forced step that fails must go back to pending.
        if not self.dry_run:
            marker.unlink(missing_ok=True)
        action()
        if not self.dry_run:
            self.directory.mkdir(parents=True, exist_ok=True)
            temporary = marker.with_suffix(".part")
            temporary.write_text("ok\n", encoding="utf-8")
            temporary.replace(marker)


def child_main():
    gate, *command = sys.argv[2:]
    deadline = time.monotonic() + 30
    while not Path(gate).is_file():
        if time.monotonic() >= deadline:
            raise SystemExit("supervisor did not release the process")
        time.sleep(0.01)
    raise SystemExit(subprocess.call(command))


if __name__ == "__main__" and len(sys.argv) > 2 and sys.argv[1] == "--child":
    child_main()
