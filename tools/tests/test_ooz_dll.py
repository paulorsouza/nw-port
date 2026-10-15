"""Smoke test of the compiled DLL without depending on the private nw-buddy fork."""
import ctypes
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nwplatform

DLL = nwplatform.ROOT / "bin/ooz.dll"


@unittest.skipUnless(nwplatform.WINDOWS and DLL.is_file(), "build ooz.dll x64 on Windows")
class OozDllTests(unittest.TestCase):
    def test_stored_block_and_failure_preserve_buffer_bounds(self):
        library = ctypes.CDLL(str(DLL))
        decompress = library.OodleLZ_Decompress
        decompress.argtypes = [ctypes.c_void_p, ctypes.c_ssize_t, ctypes.c_void_p, ctypes.c_ssize_t] + [ctypes.c_ssize_t] * 10
        decompress.restype = ctypes.c_ssize_t
        payload = b"native Windows Ooz fixture"
        source = ctypes.create_string_buffer(b"\xcc\x06" + payload)
        destination = ctypes.create_string_buffer(len(payload) + 16)
        ctypes.memset(destination, 0xcc, ctypes.sizeof(destination))
        size = decompress(source, len(payload) + 2, destination, len(payload), *([0] * 9 + [3]))
        self.assertEqual(size, len(payload))
        self.assertEqual(destination.raw[:size], payload)
        self.assertEqual(destination.raw[size:], b"\xcc" * 16)
        source[0] = b"\x00"
        self.assertEqual(decompress(source, len(payload) + 2, destination, len(payload), *([0] * 9 + [3])), 0)


if __name__ == "__main__":
    unittest.main()
