"""Conferencia estrutural leve para nao reutilizar GLBs truncados."""
import struct


def complete_glb(path):
    try:
        size = path.stat().st_size
        with path.open("rb") as stream:
            header = stream.read(12)
            if len(header) != 12:
                return False
            magic, version, length = struct.unpack("<4sII", header)
            if magic != b"glTF" or version != 2 or length != size:
                return False
            offset, chunks = 12, 0
            while offset < size:
                chunk = stream.read(8)
                if len(chunk) != 8:
                    return False
                length, kind = struct.unpack("<II", chunk)
                if length % 4 or offset + 8 + length > size or (chunks == 0 and kind != 0x4E4F534A):
                    return False
                stream.seek(length, 1)
                offset += 8 + length
                chunks += 1
            return chunks > 0 and offset == size
    except OSError:
        return False
