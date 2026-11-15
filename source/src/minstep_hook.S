.intel_syntax noprefix
.section .text
# Optional minimum local-server step ([KPCFix] LocalServerMinStepMs, default 0
# = off; Timing V3's clock_v2 is used instead). Installed in the same 35-byte
# slot client.exe 0x664b2 as clock_v2 (jmp here + nops). Context (stock code):
#   esi = server manager; [esi+0x214] = this frame's elapsed seconds, computed
#   each call as now - [esi+0x218] with [esi+0x218] = now (0x6638f-0x663a1);
#   ebx, ebp were pushed at 0x6646f/0x66470. 0x664d5 continues the stock 0.2 s
#   maximum clamp with the elapsed time in st(0); clock_v2's skip path pops
#   ebp/ebx and jumps to 0x665b2 (frame not simulated).
# A frame shorter than the minimum is not simulated; its elapsed time is
# carried into the next frame by moving [esi+0x218] back, so game time stays
# real time (no inflation) while the engine never steps below the minimum
# (the stock engine clamped to >= 10 ms, inflating time above 100 FPS).
.extern g_min_step_seconds
.global local_server_min_step
local_server_min_step:
 fld DWORD PTR [esi+0x214]
 fcom DWORD PTR g_min_step_seconds
 fnstsw ax
 test ah,0x05                 # C0 (less) or C2 (unordered): do not simulate
 jnz min_step_skip
 push 0x01d064d5              # stock maximum clamp, elapsed in st(0)
 ret
min_step_skip:
 ftst
 fnstsw ax
 test ah,0x45                 # <= 0 or NaN: nothing to carry
 jnz min_step_drop
 fsubr DWORD PTR [esi+0x218]  # last := last - elapsed (carry)
 fstp DWORD PTR [esi+0x218]
 jmp min_step_skipped
min_step_drop:
 fstp st(0)
min_step_skipped:
 mov DWORD PTR [esi+0x1f0],0
 pop ebp
 pop ebx
 push 0x01d065b2              # clock_v2's skip target
 ret
