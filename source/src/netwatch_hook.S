.intel_syntax noprefix
.section .text
# Server-connection timeout (netwatch.c). Replaces the 14 bytes at client.exe
# 0x4b4fc inside CNetMgr::Update (ebp frame: [ebp+8] = manager name,
# esi = connection, conn+0x30 = driver, conn+0x74 = idle seconds,
# driver+0x4c = timeout seconds, 120.0f for the remote TCP driver):
#   mov ecx,[esi+0x30]; fld [esi+0x74]; fcomp [ecx+0x4c]; fnstsw ax; test ah,41h
# The original "jne 0x4b53f" (idle <= timeout: no disconnect) follows the call.
# The shorter KPCFix timeout applies only to the client's own manager
# ("Client: " at 0x1d4053c), a remote driver with the stock 120 s timeout, a
# non-local connection, and an in-world client (kpc_net_armed). Everything
# else compares against the original driver timeout.
.extern kpc_net_armed
.extern g_net_timeout_seconds
.global net_timeout_compare
net_timeout_compare:
 pushad
 call kpc_net_armed
 mov DWORD PTR [esp+28],eax
 popad
 mov ecx,DWORD PTR [esi+0x30]
 fld DWORD PTR [esi+0x74]
 test eax,eax
 jz stock_timeout
 cmp DWORD PTR [ebp+8],0x01d4053c
 jne stock_timeout
 cmp DWORD PTR [ecx+0x4c],0x42f00000
 jne stock_timeout
 test BYTE PTR [esi+0x60],1
 jnz stock_timeout
 fcomp DWORD PTR g_net_timeout_seconds
 jmp timeout_compared
stock_timeout:
 fcomp DWORD PTR [ecx+0x4c]
timeout_compared:
 fnstsw ax
 test ah,0x41
 ret
