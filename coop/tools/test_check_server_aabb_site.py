"""Synthetic, license-safe tests for the original server AABB signature guard."""
import hashlib
import struct
import unittest
import check_server_aabb_site as abi

def example_pe():
    blob=bytearray(0x20200)
    blob[:2]=b"MZ"
    struct.pack_into("<I",blob,0x3c,0x80)
    blob[0x80:0x84]=b"PE\0\0"
    struct.pack_into("<H",blob,0x84,0x14c)
    struct.pack_into("<H",blob,0x86,1)
    struct.pack_into("<H",blob,0x94,0xe0)
    struct.pack_into("<H",blob,0x98,0x10b)
    s=0x80+24+0xe0
    blob[s:s+8]=b".text\0\0\0"
    struct.pack_into("<IIII",blob,s+8,0x20000,0x1000,0x20000,0x200)
    def raw(rva):
        return 0x200+rva-0x1000
    fn=b"\x90"*(abi.FUNCTION_LENGTH-1)+b"\xc3"
    blob[raw(abi.FUNCTION_RVA):raw(abi.FUNCTION_RVA)+len(fn)]=fn
    for rva in abi.CALLSITE_RVAS:
        code=b"\xe8"+struct.pack("<i",abi.FUNCTION_RVA-rva-5)+b"\x83\xc4\x14\x85\xc0"
        blob[raw(rva):raw(rva)+10]=code
    return blob,hashlib.sha256(fn).hexdigest(),raw

class TestSignatureGuard(unittest.TestCase):
    def test_synthetic_callers(self):
        blob,digest,_=example_pe()
        self.assertTrue(abi.verify_bytes(blob,original_digest=False,function_digest=digest))
    def test_original_hash_is_required_by_default(self):
        blob,digest,_=example_pe()
        with self.assertRaisesRegex(abi.SignatureError,"exact supported"):
            abi.verify_bytes(blob,function_digest=digest)
    def test_function_mutation_fails(self):
        blob,digest,raw=example_pe()
        blob[raw(abi.FUNCTION_RVA)+4]^=1
        with self.assertRaisesRegex(abi.SignatureError,"bytes differ"):
            abi.verify_bytes(blob,original_digest=False,function_digest=digest)
    def test_second_call_target_mutation_fails(self):
        blob,digest,raw=example_pe()
        blob[raw(abi.CALLSITE_RVAS[1])+1]^=1
        with self.assertRaisesRegex(abi.SignatureError,"call target"):
            abi.verify_bytes(blob,original_digest=False,function_digest=digest)
    def test_caller_cleanup_mutation_fails(self):
        blob,digest,raw=example_pe()
        blob[raw(abi.CALLSITE_RVAS[2])+7]^=1
        with self.assertRaisesRegex(abi.SignatureError,"stack cleanup"):
            abi.verify_bytes(blob,original_digest=False,function_digest=digest)
    def test_wrong_format_fails(self):
        blob,digest,_=example_pe()
        blob[0]^=1
        with self.assertRaisesRegex(abi.SignatureError,"MZ"):
            abi.verify_bytes(blob,original_digest=False,function_digest=digest)

if __name__=="__main__":
    unittest.main()
