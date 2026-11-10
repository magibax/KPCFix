#!/usr/bin/env python3
"""Read-only guard for the proposed KPC 1.13 spawn-position tracing hook.

Checks original object.lto in the local installation. Never modifies inputs.
The executable source in the official game is not distributed with this tool.
"""
from __future__ import annotations
import argparse
import hashlib
import struct
from pathlib import Path

ORIGINAL_SHA256 = '3a03f8796c482b1a7ad72755df33ee8f1f8056bb10957f6f700e64bd006b3e5b'
FALLBACK_MSG_RVA = 0x148A7C
FALLBACK_PUSH_RVA = 0xF12F1
FALLBACK_SELECTOR_CALL_RVA = 0xF1339
INITIAL_SELECTOR_CALL_RVA = 0xF12BE
SPAWN_ENGINE_CALL_RVA = 0xF13C4
SPAWN_ENGINE_CALL_ORIG = bytes.fromhex('ff91e0010000')


def inspect(data: bytes, require_hash=True):
    digest = hashlib.sha256(data).hexdigest()
    if require_hash and digest != ORIGINAL_SHA256:
        raise ValueError('Official original object.lto hash mismatch')
    if len(data) < 0x151000:
        raise ValueError('Object image truncated')
    if data[:2] != b'MZ':
        raise ValueError('Not a PE image')
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    if data[pe:pe+4] != b'PE\x00\x00':
        raise ValueError('Bad PE signature')
    # In this supported build, section file offsets equal RVAs at these sites.
    def require(rva, expected, label):
        actual = data[rva:rva+len(expected)]
        if actual != expected:
            raise ValueError(f'{label} at RVA {rva:#x}: expected {expected.hex()}, got {actual.hex()}')
    require(FALLBACK_MSG_RVA, b'Error!! Could not find any multiplayer start points.', 'fallback string')
    require(FALLBACK_PUSH_RVA, bytes.fromhex('687c8a1410'), 'fallback string reference')
    require(INITIAL_SELECTOR_CALL_RVA, bytes.fromhex('e8adfdffff'), 'initial start selector')
    require(FALLBACK_SELECTOR_CALL_RVA, bytes.fromhex('e862f5ffff'), 'fallback single-player selector')
    require(0xF132B, bytes.fromhex('8db9d0010000'), 'game type filter address')
    require(0xF1333, bytes.fromhex('c70700000000'), 'temporary zero game type')
    require(0xF1340, bytes.fromhex('891f'), 'restore previous game type')
    require(0xF1355, bytes.fromhex('ff92d0010000'), 'read start position via API')
    require(0xF13A4, bytes.fromhex('e817fdffff'), 'spawn transform setup')
    require(0xF10C0, bytes.fromhex('81ec80000000'), 'spawn preparation entry')
    require(0xF129D, bytes.fromhex('c20c00'), 'spawn preparation thiscall cleanup')
    require(SPAWN_ENGINE_CALL_RVA, SPAWN_ENGINE_CALL_ORIG, 'player placement API call')
    require(0xF15F2, bytes.fromhex('e8a9f2ffff'), 'secondary spawn selector')
    require(0xF167A, bytes.fromhex('e841faffff'), 'secondary pre-overlap handler')
    require(0xF16C7, bytes.fromhex('ff92e0010000'), 'secondary position setter')
    require(0xF13CA, bytes.fromhex('83c408'), 'caller cleans two position arguments')

    # Original KPC 1.13 IntersectSegment CDECL, not inferred from a later SDK.
    require(0xF2FAE, bytes.fromhex('ff9090000000'), 'intersect interface slot +0x90')
    require(0xF2FB4, bytes.fromhex('83c4083c01'), 'intersect caller cleanup and bool return')
    require(0xF2F5F, bytes.fromhex('c78424d000000005000000'), 'intersect flags at +0x24')
    require(0xF2F6A, bytes.fromhex('c78424d400000030271010'), 'intersect filter at +0x28')
    require(0xF2F75, bytes.fromhex('89b424d8000000'), 'intersect filter userdata at +0x2c')
    require(0x3A60, bytes.fromhex('8bc133c9894824894828c3'), 'vintage query flags/filter initializer')
    require(0x102730, bytes.fromhex('8b44240485c0750332c0c33b4424080f95c0c3'), 'original cdecl object filter')
    require(0xEBC7, bytes.fromhex('ff909000000083c408'), 'independent intersect caller')

    return {
        'sha256': digest,
        'spawn_object_base': '0x10000000',
        'start_selector_rva': '0x0f1070',
        'spawn_initialization_rva': '0x0f12a0',
        'spawn_position_call_rva': '0x0f13c4',
        'secondary_spawn_path_rva': '0x0f15d0',
        'intersect_segment_api_offset': '0x90',
        'intersect_callsite_rva': '0x0f2fae',
        'intersect_query_vintage_bytes': 48,
        'spawn_position_api_field_offset': '0x01e0',
        'guards_passed': 24,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('object_lto', type=Path)
    args = ap.parse_args()
    result = inspect(args.object_lto.read_bytes())
    for key, value in result.items():
        print(f'{key}: {value}')


if __name__ == '__main__':
    main()
