"""Read-only signature guard for original 1.13 server.dll BSP point walker.

No game binary is executed or modified. Never port raw offsets to other builds.
"""
import argparse
import hashlib
import struct
from pathlib import Path
from check_server_aabb_site import pe_rva_to_raw, SignatureError

ORIGINAL_SHA256 = "83b7bce809948b6f70ed3f35a77a771b6087f1104d90d06a62ac3dc0d4bd37d5"
WALKER_RVA = 0x1068f
WALKER_SIZE = 0x4b
WALKER_SHA256 = "8f9f9e17e82c130e85704585e66c8fff62b7de782e0c79d8eb29419eba64b35f"
CALLSITE_RVAS = (0x24426,0x28fdc,0x2e2f,0x2e3d,0x2e4a)
CALLER_VTABLE_RVA = 0x416f4
CALLER_METHOD_OFFSET = 0x1c
METHOD_RVA = 0x2440a

def verify_bytes(blob, original_digest=True, walker_digest=WALKER_SHA256):
    if original_digest and hashlib.sha256(blob).hexdigest()!=ORIGINAL_SHA256:
        raise SignatureError("not the exact supported original 1.13 server")
    at=pe_rva_to_raw(blob,WALKER_RVA,WALKER_SIZE)
    code=blob[at:at+WALKER_SIZE]
    if hashlib.sha256(code).hexdigest()!=walker_digest:
        raise SignatureError("native BSP point-walker body changed")
    if code[-1]!=0xc3 or code[:4]!=bytes.fromhex("8b542408"):
        raise SignatureError("native BSP walker entry/return changed")
    for site in CALLSITE_RVAS:
        pos=pe_rva_to_raw(blob,site,5)
        if blob[pos]!=0xe8 or site+5+struct.unpack_from("<i",blob,pos+1)[0]!=WALKER_RVA:
            raise SignatureError(f"point walker call changed at RVA 0x{site:x}")
    method=pe_rva_to_raw(blob,CALLER_VTABLE_RVA+CALLER_METHOD_OFFSET,4)
    if struct.unpack_from("<I",blob,method)[0]!=0x10000000+METHOD_RVA:
        raise SignatureError("GetPointStatus vtable entry changed")
    entry=pe_rva_to_raw(blob,METHOD_RVA,0x2b)
    if blob[entry+0x1c]!=0xe8 or METHOD_RVA+0x1c+5+struct.unpack_from("<i",blob,entry+0x1d)[0]!=WALKER_RVA:
        raise SignatureError("GetPointStatus wrapper call changed")
    return True

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("server_original",type=Path)
    a=p.parse_args()
    try: verify_bytes(a.server_original.read_bytes())
    except (OSError,SignatureError) as exc: p.exit(1,f"FAIL: {exc}\n")
    print("PASS: original 1.13 BSP leaf walker, five original calls and GetPointStatus vtable")
    print("WARNING: point classifier is not a native full-hull world-clearance result")

if __name__=="__main__":
    main()
