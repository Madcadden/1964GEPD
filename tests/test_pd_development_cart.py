#!/usr/bin/env python3
"""Run the production loaded-ROM helper on supplied ROMs, without altering files.

Usage: python tests/test_pd_development_cart.py EC.rom DC.rom PAL.rom retail.z64
Requires a C compiler and zlib development library. No ROM data is distributed.
"""
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zlib


def big_endian(path):
    data = bytearray(Path(path).read_bytes())
    if data[:4] == bytes.fromhex('37804012'):
        data[0::2], data[1::2] = data[1::2], data[0::2]
    elif data[:4] == bytes.fromhex('40123780'):
        data = word_swap(data)
    assert data[:4] == bytes.fromhex('80371240')
    return data


def word_swap(data):
    result = bytearray(len(data))
    for i in range(4):
        result[i::4] = data[3-i::4]
    return result


def main():
    source = Path(__file__).resolve().parents[1]
    paths = sys.argv[1:]
    assert len(paths) >= 4, __doc__
    originals = {p: hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in paths}
    ec, dc, pal, retail = [big_endian(p) for p in paths[:4]]
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        (temp / 'helper.c').write_text('#include <zlib.h>\n#include "PerfectDarkPrototype.h"\n'
                                     'int normalize(unsigned char *r,size_t n) {return PDNormalizeDevelopmentCart(r,n);}\n')
        subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-O2', '-shared',
                        '-fPIC', '-I', str(source), str(temp / 'helper.c'), '-lz',
                        '-o', str(temp / 'helper.so')], check=True)
        normalize = ctypes.CDLL(str(temp / 'helper.so')).normalize
        normalize.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        normalize.restype = ctypes.c_int

        def run(data, expected, size=None):
            internal = word_swap(data)
            buffer = (ctypes.c_ubyte * len(internal)).from_buffer(internal)
            before = hashlib.sha256(internal).digest()
            assert normalize(buffer, len(internal) if size is None else size) == expected
            if not expected:
                assert hashlib.sha256(internal).digest() == before
            return word_swap(internal)

        assert normalize(None, 0) == 0
        run(ec[:64], 0)
        run(ec, 0, len(ec)-4)
        run(dc, 0)
        run(pal, 0)
        run(retail, 0)
        broken_header = ec.copy()
        broken_header[0x25] ^= 1
        run(broken_header, 0)
        broken_code = ec.copy()
        broken_code[0x8000] ^= 1
        run(broken_code, 0)
        broken_length = ec.copy()
        broken_length[0x3054] ^= 1
        run(broken_length, 0)
        fixed = run(ec, 1)
        run(fixed, 0)  # idempotent: the normalized image is not an EC match
        # Only checksum/identity and the allocated library stream may change.
        assert fixed[:0x10] == ec[:0x10]
        assert fixed[0x18:0x20] == ec[0x18:0x20]
        assert fixed[0x40:0x3055] == ec[0x40:0x3055]
        assert fixed[0x30850:] == ec[0x30850:]
        assert fixed[0x20:0x40] == dc[0x20:0x40]
        original_lib = zlib.decompress(ec[0x3055:], -15)
        fixed_lib = zlib.decompress(fixed[0x3055:], -15)
        dc_lib = zlib.decompress(dc[0x3055:], -15)
        assert fixed_lib == dc_lib
        assert sum(a != b for a, b in zip(original_lib, fixed_lib)) == 8
        for p, sha in originals.items():
            assert hashlib.sha256(Path(p).read_bytes()).hexdigest() == sha
        print(json.dumps({'result': 'pass', 'cases': 12,
                          'fixed_library_matches_dc': True,
                          'original_rom_files_unchanged': True,
                          'fixed_crc_header': fixed[0x10:0x18].hex(),
                          'input_sha256': originals}, indent=2))


if __name__ == '__main__':
    main()
