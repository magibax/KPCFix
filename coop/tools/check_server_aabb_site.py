"""Read-only original 1.13 server AABB comparator signature guard.

Usage: python coop/tools/check_server_aabb_site.py /path/to/server.original.dll
This tool never executes or changes game binaries, and does not redistribute
the original 114-byte commercial function body.
"""
import argparse
import hashlib
import struct
from pathlib import Path

ORIGINAL_SERVER_SHA256 = "83b7bce809948b6f70ed3f35a77a771b6087f1104d90d06a62ac3dc0d4bd37d5"
FUNCTION_RVA = 0x18991
FUNCTION_LENGTH = 0x72
FUNCTION_SHA256 = "1e3021c4b29680f9f06f43424b61d937f32aa33b7749c32c62abc0472588fa1f"
CALLSITE_RVAS = (0x186B9, 0x1953F, 0x1A915)

class SignatureError(ValueError):
    pass

def _u16(blob, offset):
    if offset < 0 or offset+2 > len(blob):
        raise SignatureError("invalid PE field")
    return struct.unpack_from("<H",blob,offset)[0]

def _u32(blob, offset):
    if offset < 0 or offset+4 > len(blob):
        raise SignatureError("invalid PE field")
    return struct.unpack_from("<I",blob,offset)[0]

def pe_rva_to_raw(blob, rva, size):
    if len(blob)<0x100 or blob[:2]!=b"MZ":
        raise SignatureError("not an MZ executable")
    pe=_u32(blob,0x3c)
    if pe>len(blob)-0x100 or blob[pe:pe+4]!=b"PE\0\0":
        raise SignatureError("invalid PE header")
    if _u16(blob,pe+4)!=0x14c or _u16(blob,pe+24)!=0x10b:
        raise SignatureError("not a PE32 x86 image")
    count=_u16(blob,pe+6)
    sections=pe+24+_u16(blob,pe+20)
    if not 0<count<=32 or sections+40*count>len(blob):
        raise SignatureError("invalid section table")
    for i in range(count):
        s=sections+40*i
        base=_u32(blob,s+12)
        length=_u32(blob,s+16)
        raw=_u32(blob,s+20)
        if rva>=base and rva+size<=base+length:
            offset=raw+rva-base
            if offset+size>len(blob):
                raise SignatureError("section missing on disk")
            return offset
    raise SignatureError(f"RVA 0x{rva:x} not in file-backed section")

def verify_bytes(blob, *, original_digest=True, function_digest=FUNCTION_SHA256):
    if original_digest and hashlib.sha256(blob).hexdigest()!=ORIGINAL_SERVER_SHA256:
        raise SignatureError("not the exact supported original European-family 1.13 server.dll")
    start=pe_rva_to_raw(blob,FUNCTION_RVA,FUNCTION_LENGTH)
    code=blob[start:start+FUNCTION_LENGTH]
    if hashlib.sha256(code).hexdigest()!=function_digest:
        raise SignatureError("AABB overlap comparator bytes differ from verified original")
    if code[-1:]!=b"\xc3":
        raise SignatureError("AABB comparator no longer returns directly")
    for rva in CALLSITE_RVAS:
        offset=pe_rva_to_raw(blob,rva,10)
        call=blob[offset:offset+10]
        if call[0]!=0xe8 or rva+5+struct.unpack_from("<i",call,1)[0]!=FUNCTION_RVA:
            raise SignatureError(f"AABB call target changed at RVA 0x{rva:x}")
        if call[5:10]!=b"\x83\xc4\x14\x85\xc0":
            raise SignatureError(f"expected caller stack cleanup/return check missing at RVA 0x{rva:x}")
    return True

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("server_original",type=Path)
    args=parser.parse_args()
    try:
        verify_bytes(args.server_original.read_bytes())
    except (OSError,SignatureError) as exc:
        parser.exit(1,f"FAIL: {exc}\n")
    print("PASS: original 1.13 server AABB comparator at RVA 0x18991")
    print("PASS: three independent relative call targets, cdecl 20-byte cleanup")
    print("NOTE: box-to-box broadphase is not full BSP/world clearance")

if __name__=="__main__":
    main()
