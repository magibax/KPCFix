.intel_syntax noprefix
.section .text
.extern g_original_ftol
.extern g_carries
.global kpctim_begin
kpctim_begin:
.global carry_0
carry_0:
 fxam
 fnstsw ax
 test ah,1
 jnz reset_0
 mov al,ah
 and al,0x45
 cmp al,0x40
 je pass_0
 test ah,2
 jnz reset_0
 fcom QWORD PTR [max_ms]
 fnstsw ax
 test ah,1
 jz reset_0
 sub esp,4
 fnstcw WORD PTR [esp]
 mov ax,WORD PTR [esp]
 or ax,0x300
 mov WORD PTR [esp+2],ax
 fldcw WORD PTR [esp+2]
 fadd QWORD PTR ds:g_carries+0
 fld st(0)
 call DWORD PTR [g_original_ftol]
 push edx
 push eax
 fild QWORD PTR [esp]
 fsubp st(1),st(0)
 fstp QWORD PTR ds:g_carries+0
 add esp,8
 fldcw WORD PTR [esp]
 add esp,4
 ret
reset_0:
 fldz
 fstp QWORD PTR ds:g_carries+0
pass_0:
 jmp DWORD PTR [g_original_ftol]
.balign 16,0x90
.global carry_1
carry_1:
 fxam
 fnstsw ax
 test ah,1
 jnz reset_1
 mov al,ah
 and al,0x45
 cmp al,0x40
 je pass_1
 test ah,2
 jnz reset_1
 fcom QWORD PTR [max_ms]
 fnstsw ax
 test ah,1
 jz reset_1
 sub esp,4
 fnstcw WORD PTR [esp]
 mov ax,WORD PTR [esp]
 or ax,0x300
 mov WORD PTR [esp+2],ax
 fldcw WORD PTR [esp+2]
 fadd QWORD PTR ds:g_carries+8
 fld st(0)
 call DWORD PTR [g_original_ftol]
 push edx
 push eax
 fild QWORD PTR [esp]
 fsubp st(1),st(0)
 fstp QWORD PTR ds:g_carries+8
 add esp,8
 fldcw WORD PTR [esp]
 add esp,4
 ret
reset_1:
 fldz
 fstp QWORD PTR ds:g_carries+8
pass_1:
 jmp DWORD PTR [g_original_ftol]
.balign 16,0x90
.global carry_2
carry_2:
 fxam
 fnstsw ax
 test ah,1
 jnz reset_2
 mov al,ah
 and al,0x45
 cmp al,0x40
 je pass_2
 test ah,2
 jnz reset_2
 fcom QWORD PTR [max_ms]
 fnstsw ax
 test ah,1
 jz reset_2
 sub esp,4
 fnstcw WORD PTR [esp]
 mov ax,WORD PTR [esp]
 or ax,0x300
 mov WORD PTR [esp+2],ax
 fldcw WORD PTR [esp+2]
 fadd QWORD PTR ds:g_carries+16
 fld st(0)
 call DWORD PTR [g_original_ftol]
 push edx
 push eax
 fild QWORD PTR [esp]
 fsubp st(1),st(0)
 fstp QWORD PTR ds:g_carries+16
 add esp,8
 fldcw WORD PTR [esp]
 add esp,4
 ret
reset_2:
 fldz
 fstp QWORD PTR ds:g_carries+16
pass_2:
 jmp DWORD PTR [g_original_ftol]
.balign 16,0x90
.global carry_3
carry_3:
 fxam
 fnstsw ax
 test ah,1
 jnz reset_3
 mov al,ah
 and al,0x45
 cmp al,0x40
 je pass_3
 test ah,2
 jnz reset_3
 fcom QWORD PTR [max_ms]
 fnstsw ax
 test ah,1
 jz reset_3
 sub esp,4
 fnstcw WORD PTR [esp]
 mov ax,WORD PTR [esp]
 or ax,0x300
 mov WORD PTR [esp+2],ax
 fldcw WORD PTR [esp+2]
 fadd QWORD PTR ds:g_carries+24
 fld st(0)
 call DWORD PTR [g_original_ftol]
 push edx
 push eax
 fild QWORD PTR [esp]
 fsubp st(1),st(0)
 fstp QWORD PTR ds:g_carries+24
 add esp,8
 fldcw WORD PTR [esp]
 add esp,4
 ret
reset_3:
 fldz
 fstp QWORD PTR ds:g_carries+24
pass_3:
 jmp DWORD PTR [g_original_ftol]
.balign 16,0x90
.global carry_4
carry_4:
 fxam
 fnstsw ax
 test ah,1
 jnz reset_4
 mov al,ah
 and al,0x45
 cmp al,0x40
 je pass_4
 test ah,2
 jnz reset_4
 fcom QWORD PTR [max_ms]
 fnstsw ax
 test ah,1
 jz reset_4
 sub esp,4
 fnstcw WORD PTR [esp]
 mov ax,WORD PTR [esp]
 or ax,0x300
 mov WORD PTR [esp+2],ax
 fldcw WORD PTR [esp+2]
 fadd QWORD PTR ds:g_carries+32
 fld st(0)
 call DWORD PTR [g_original_ftol]
 push edx
 push eax
 fild QWORD PTR [esp]
 fsubp st(1),st(0)
 fstp QWORD PTR ds:g_carries+32
 add esp,8
 fldcw WORD PTR [esp]
 add esp,4
 ret
reset_4:
 fldz
 fstp QWORD PTR ds:g_carries+32
pass_4:
 jmp DWORD PTR [g_original_ftol]
.balign 16,0x90
.global carry_5
carry_5:
 fxam
 fnstsw ax
 test ah,1
 jnz reset_5
 mov al,ah
 and al,0x45
 cmp al,0x40
 je pass_5
 test ah,2
 jnz reset_5
 fcom QWORD PTR [max_ms]
 fnstsw ax
 test ah,1
 jz reset_5
 sub esp,4
 fnstcw WORD PTR [esp]
 mov ax,WORD PTR [esp]
 or ax,0x300
 mov WORD PTR [esp+2],ax
 fldcw WORD PTR [esp+2]
 fadd QWORD PTR ds:g_carries+40
 fld st(0)
 call DWORD PTR [g_original_ftol]
 push edx
 push eax
 fild QWORD PTR [esp]
 fsubp st(1),st(0)
 fstp QWORD PTR ds:g_carries+40
 add esp,8
 fldcw WORD PTR [esp]
 add esp,4
 ret
reset_5:
 fldz
 fstp QWORD PTR ds:g_carries+40
pass_5:
 jmp DWORD PTR [g_original_ftol]
.balign 16,0x90
.global carry_6
carry_6:
 fxam
 fnstsw ax
 test ah,1
 jnz reset_6
 mov al,ah
 and al,0x45
 cmp al,0x40
 je pass_6
 test ah,2
 jnz reset_6
 fcom QWORD PTR [max_ms]
 fnstsw ax
 test ah,1
 jz reset_6
 sub esp,4
 fnstcw WORD PTR [esp]
 mov ax,WORD PTR [esp]
 or ax,0x300
 mov WORD PTR [esp+2],ax
 fldcw WORD PTR [esp+2]
 fadd QWORD PTR ds:g_carries+48
 fld st(0)
 call DWORD PTR [g_original_ftol]
 push edx
 push eax
 fild QWORD PTR [esp]
 fsubp st(1),st(0)
 fstp QWORD PTR ds:g_carries+48
 add esp,8
 fldcw WORD PTR [esp]
 add esp,4
 ret
reset_6:
 fldz
 fstp QWORD PTR ds:g_carries+48
pass_6:
 jmp DWORD PTR [g_original_ftol]
.balign 16,0x90
.global carry_7
carry_7:
 fxam
 fnstsw ax
 test ah,1
 jnz reset_7
 mov al,ah
 and al,0x45
 cmp al,0x40
 je pass_7
 test ah,2
 jnz reset_7
 fcom QWORD PTR [max_ms]
 fnstsw ax
 test ah,1
 jz reset_7
 sub esp,4
 fnstcw WORD PTR [esp]
 mov ax,WORD PTR [esp]
 or ax,0x300
 mov WORD PTR [esp+2],ax
 fldcw WORD PTR [esp+2]
 fadd QWORD PTR ds:g_carries+56
 fld st(0)
 call DWORD PTR [g_original_ftol]
 push edx
 push eax
 fild QWORD PTR [esp]
 fsubp st(1),st(0)
 fstp QWORD PTR ds:g_carries+56
 add esp,8
 fldcw WORD PTR [esp]
 add esp,4
 ret
reset_7:
 fldz
 fstp QWORD PTR ds:g_carries+56
pass_7:
 jmp DWORD PTR [g_original_ftol]
.balign 16,0x90
max_ms:
.double 2147483648.0
.section .note.GNU-stack,"",@progbits
