.intel_syntax noprefix
.section .text
.global probe_entry, probe_end

# Called in place of: MOV EAX,[ESI+38]; PUSH EAX; CALL [EDX+40].
# On entry: [ESP]=return, [ESP+4]=target position, [ESP+8]=flags.
# Preserve the full incoming machine state while adjusting only target Y.
probe_entry:
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cmp DWORD PTR [esi+0x54],1  # grounded, with container-mode bytes 55..57 clear
 jne .Ldone
 test DWORD PTR [esi+0x44],4
 jne .Ldone
 cmp DWORD PTR [esi+0x4c],0
 jne .Ldone
 mov edx,DWORD PTR [esi+0x38]
 test edx,edx
 jz .Ldone
 test DWORD PTR [edx+0x28],0x20000  # native FLAG_GRAVITY path only
 jz .Ldone
 mov edi,DWORD PTR [ebp+40]
 test edi,edi
 jz .Ldone
 mov eax,DWORD PTR [edx+0x80]
 mov ecx,eax
 and ecx,0x7fffffff
 cmp ecx,0x7f800000
 jae .Ldone
 test ecx,ecx
 jz .Lvelocity_ok
 test eax,eax
 jns .Ldone
.Lvelocity_ok:
 mov eax,DWORD PTR [edx+0x44]
 and eax,0x7fffffff
 cmp eax,0x7f800000
 jae .Ldone
 mov eax,DWORD PTR [edi+4]
 and eax,0x7fffffff
 cmp eax,0x7f800000
 jae .Ldone
 # Use a clean temporary x87 stack, retaining the caller's precision/rounding.
 fninit
 fldcw WORD PTR [esp]
 fld DWORD PTR [edx+0x44]
 fld DWORD PTR [edi+4]
 fcom st(1)
 fnstsw ax
 test ah,0x41
 je .Ldone                  # target rises: never pull it down
 fstp st(0)
 mov DWORD PTR [esp+512],0x3d800000  # 1/16 game unit
 fsub DWORD PTR [esp+512]
 fld DWORD PTR [edi+4]
 fcom st(1)
 fnstsw ax
 test ah,0x41
 jne .Ldone                 # original request already goes farther down
 fstp st(0)
 fstp DWORD PTR [edi+4]
.Ldone:
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 # Reproduce the original dispatch with its original ECX and EDX.
 mov eax,DWORD PTR [esi+0x38]
 push DWORD PTR [esp+8]      # copy original flags
 push DWORD PTR [esp+8]      # copy original target pointer after that push
 push eax
 call DWORD PTR [edx+0x40]
 ret 8                      # consume original target/flags, as native call did
probe_end:
