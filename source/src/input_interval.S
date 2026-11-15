.intel_syntax noprefix
.section .text
.global input_interval_guard,input_interval_end
.extern g_input_late_events,g_input_zero_intervals,g_input_first_late_delta

# Called from client+344A5; return address adds four bytes to its local offsets.
# ESI=axis, EDI=DIDEVICEOBJECTDATA+4, EBX=axis type. No FPU instructions.
# EAX returns a nonnegative event interval; EDX retains native frame interval.
input_interval_guard:
 mov edx,DWORD PTR [esp+0x28]
 lea eax,[edx+edx]
 cmp DWORD PTR [esi+0x100],eax
 jae .Lwithout_minimum
 mov eax,DWORD PTR [edi+4]
 sub eax,DWORD PTR [esi+0xfc]
 jns .Lwith_minimum
 cmp DWORD PTR [g_input_late_events],0
 jne .Llate_minimum
 mov DWORD PTR [g_input_first_late_delta],eax
.Llate_minimum:
 xor eax,eax
 inc DWORD PTR [g_input_late_events]
.Lwith_minimum:
 mov ecx,DWORD PTR ds:0x01d4b668
 cmp eax,ecx
 ja .Lseed
 mov eax,ecx
 jmp .Lseed
.Lwithout_minimum:
 mov eax,DWORD PTR [edi+4]
 sub eax,DWORD PTR [esi+0xfc]
 jns .Lseed
 cmp DWORD PTR [g_input_late_events],0
 jne .Llate_no_minimum
 mov DWORD PTR [g_input_first_late_delta],eax
.Llate_no_minimum:
 xor eax,eax
 inc DWORD PTR [g_input_late_events]
.Lseed:
 test eax,eax
 jnz .Ldone
 cmp DWORD PTR [esi+0x100],0
 jne .Ldone
 inc eax
 inc DWORD PTR [g_input_zero_intervals]
.Ldone:
 ret
input_interval_end:
.section .note.GNU-stack,"",@progbits
