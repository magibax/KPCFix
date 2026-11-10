"""Asset-free mutation tests for original engine BSP point-walker signatures."""
import hashlib
import struct
import unittest
import check_server_bsp_site as bsp
from test_check_server_aabb_site import example_pe

def fixture():
    b,_,raw=example_pe()
    # The shared synthetic AABB fixture originally maps only a 0x20000-byte
    # text section. BSP's GetPointStatus wrapper is at RVA 0x2440a.
    b.extend(b"\\0"*0x10000)
    section=0x80+24+0xe0
    struct.pack_into("<II",b,section+8,0x30000,0x30000)
    fn=b"\x8b\x54\x24\x08"+b"\x90"*(bsp.WALKER_SIZE-5)+b"\xc3"
    b[raw(bsp.WALKER_RVA):raw(bsp.WALKER_RVA)+len(fn)]=fn
    for site in bsp.CALLSITE_RVAS:
        b[raw(site):raw(site)+5]=b"\xe8"+struct.pack("<i",bsp.WALKER_RVA-site-5)
    struct.pack_into("<I",b,raw(bsp.CALLER_VTABLE_RVA+bsp.CALLER_METHOD_OFFSET),
                     0x10000000+bsp.METHOD_RVA)
    site=raw(bsp.METHOD_RVA)
    b[site:site+0x2b]=b"\x90"*0x2b
    b[site+0x1c:site+0x21]=b"\xe8"+struct.pack("<i",bsp.WALKER_RVA-(bsp.METHOD_RVA+0x1c)-5)
    return b,hashlib.sha256(fn).hexdigest(),raw

class TestNativeBspSignatures(unittest.TestCase):
    def test_valid_synthetic(self):
        b,h,_=fixture()
        self.assertTrue(bsp.verify_bytes(b,original_digest=False,walker_digest=h))
    def test_requires_real_binary_hash_by_default(self):
        b,h,_=fixture()
        with self.assertRaisesRegex(bsp.SignatureError,"exact supported"):
            bsp.verify_bytes(b,walker_digest=h)
    def test_fails_mutated_walker(self):
        b,h,raw=fixture();b[raw(bsp.WALKER_RVA)+7]^=1
        with self.assertRaisesRegex(bsp.SignatureError,"walker body"):
            bsp.verify_bytes(b,original_digest=False,walker_digest=h)
    def test_fails_mutated_callsite(self):
        b,h,raw=fixture();b[raw(bsp.CALLSITE_RVAS[3])+1]^=1
        with self.assertRaisesRegex(bsp.SignatureError,"walker call"):
            bsp.verify_bytes(b,original_digest=False,walker_digest=h)
    def test_fails_mutated_vtable(self):
        b,h,raw=fixture();b[raw(bsp.CALLER_VTABLE_RVA+bsp.CALLER_METHOD_OFFSET)]^=1
        with self.assertRaisesRegex(bsp.SignatureError,"vtable"):
            bsp.verify_bytes(b,original_digest=False,walker_digest=h)

if __name__=="__main__":
    unittest.main()
