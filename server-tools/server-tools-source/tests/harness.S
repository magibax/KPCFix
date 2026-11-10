.intel_syntax noprefix
.code32
.section .harness,"ax"
.global _start
.global lookup,broadcast,original_chat,forward,alloc_message
_start:
 cld
 mov dword ptr [0x16de80],offset engine
 mov dword ptr [player+8],offset physics_object
 mov byte ptr [player+0x13f],1
 mov esi,offset player
 call inv_hook
 mov byte ptr [player+0x13f],0
 call inv_hook
 push 44
 push 0x12345678
 call noclip_hook
 add esp,8
 # Client-reported collision flag transitions, not guessed toggle parity.
 push 0x200000
 push offset physics_object
 call flags_hook
 add esp,8
 # An identical report must not create a duplicate announcement.
 push 0x200000
 push offset physics_object
 call flags_hook
 add esp,8
 push 0x2000
 push offset physics_object
 call flags_hook
 add esp,8
 # A non-noclip flag change must stay silent.
 push 0
 push offset physics_object
 call flags_hook
 add esp,8
 push offset chat_text
 push offset player+0x1e8
 push offset fmt
 push 0
 push 0
 call chat_hook
 add esp,20
 cmp dword ptr [console+0x58],2
 jne fail
 cmp dword ptr [console+0x5c],6
 jne fail
 cmp dword ptr [console+4],0x74617473
 jne fail
 cmp dword ptr [targeted],1
 jne fail
 cmp dword ptr [chat_called],1
 jne fail
 cmp dword ptr [broadcasts],4
 jne fail
 # Redirected output must not contain redraw control sequences.
 mov dword ptr [interactive],0
 mov byte ptr [player+0x13f],1
 mov esi,offset player
 call inv_hook
 mov eax,1
 xor ebx,ebx
 int 0x80
fail:
 mov eax,1
 mov ebx,42
 int 0x80
alloc_message:
 mov dword ptr [word_count],0
 mov eax,offset message
 ret
write_word:
 mov eax,[word_count]
 cmp eax,0
 jne 1f
 cmp dword ptr [esp+4],4
 jne fail
 jmp 2f
1: cmp eax,1
 jne fail
 cmp dword ptr [esp+4],0
 jne fail
2: inc dword ptr [word_count]
 ret 4
write_string:
 cmp dword ptr [word_count],2
 jne fail
 mov eax,[esp+4]
 cmp byte ptr [eax],'d'
 jne fail
 mov [packet_text],eax
 ret 4
send_packet:
 cmp dword ptr [esp+4],offset message
 jne fail
 cmp dword ptr [esp+8],0x45
 jne fail
 cmp dword ptr [esp+12],0
 jne fail
 cmp dword ptr [esp+16],1
 jne fail
 cmp dword ptr [word_count],2
 jne fail
 inc dword ptr [broadcasts]
 # Sending must never call the old console-echoing broadcast stub.
 ret 16
release_message:
 ret
getflags:
 mov eax,[esp+4]
 mov eax,[eax]
 ret
setflags:
 mov eax,[esp+4]
 mov edx,[esp+8]
 mov [eax],edx
 ret
getmodule:
 mov eax,offset fakeexe
 cmp dword ptr [esp+4],0
 je 1f
 mov eax,1
1: ret 4
getproc:
 mov eax,offset getmode
 ret 8
getmode:
 mov eax,[esp+8]
 mov dword ptr [eax],1
 mov eax,[interactive]
 ret 8
getstd:
 mov eax,99
 ret 4
writefile:
 pushad
 mov ecx,[esp+40]
 mov edx,[esp+44]
 mov eax,4
 mov ebx,1
 int 0x80
 popad
 mov eax,1
 ret 20
broadcast:
 inc dword ptr [broadcasts]
 cmp dword ptr [esp+4],0
 jne fail
 cmp dword ptr [esp+12],5
 jne fail
 cmp dword ptr [esp+16],0x40a00000
 jne fail
 pushad
 mov ecx,[esp+56]
 call print_broadcast
 popad
 ret
print_broadcast:
 xor edx,edx
1: cmp byte ptr [ecx+edx],0
 je 2f
 inc edx
 jmp 1b
2: mov eax,4
 mov ebx,1
 int 0x80
 mov eax,4
 mov ebx,1
 mov ecx,offset nl
 mov edx,1
 int 0x80
 ret
forward:
 cmp dword ptr [esp+4],0x12345678
 jne fail
 cmp dword ptr [esp+8],44
 jne fail
 inc dword ptr [targeted]
 ret
lookup:
 cmp dword ptr [esp+4],0x12345678
 jne fail
 mov eax,offset player
 ret
original_chat:
 cmp dword ptr [esp+16],offset player+0x1e8
 jne fail
 cmp dword ptr [esp+20],offset chat_text
 jne fail
 inc dword ptr [chat_called]
 ret
.section .iat,"aw"
.org 0x54
.long getproc,getmodule
.org 0x98
.long writefile
.org 0xcc
.long getstd
.section .fakeexe,"aw"
fakeexe:
.word 0x5a4d
.org 0x3c
.long 0x100
.org 0x100
.long 0x4550,0,0x39b907ab
.org 0x150
.long 0x18000
.org 0x1c5a
.byte 0xb9
.long console
.byte 0xe8
.org 0x12f70
console:
.long 0
.asciz "status"
.org 0x12f70+0x58
.long 2,6,1
.asciz ">"
.org 0x18000
.section .testdata,"aw"
player:
.space 0x1e8
.asciz "darnell"
.space 64
interactive: .long 1
targeted: .long 0
chat_called: .long 0
broadcasts: .long 0
chat_text: .asciz "hello 100%\nthere"
fmt: .asciz "%s %s"
nl: .ascii "\n"

.balign 4
physics_object: .long 0
engine:
.long engine_vtable
.space 0x1f4
.long getflags,setflags

engine_vtable:
.space 0xc4
.long send_packet
message: .long message_vtable
message_vtable:
.long release_message
.space 0x38
.long write_word
.space 0xc
.long write_string
word_count: .long 0
packet_text: .long 0
