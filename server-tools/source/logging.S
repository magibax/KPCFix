.intel_syntax noprefix
.code32
.global inv_hook,noclip_hook
inv_hook:
 pushfd
 pushad
 call 1f
1: pop ebp
 sub ebp, offset 1b
 lea edx,[ebp+inv_off]
 cmp byte ptr [esi+0x13f],0
 je 2f
 lea edx,[ebp+inv_on]
2: call write_log
 popad
 popfd
 # Preserve original diagnostic print call and its argument.
 ret
noclip_hook:
 push dword ptr [esp+8]
 push dword ptr [esp+8]
 call 0xf96f0
 add esp,8
 pushfd
 pushad
 call 3f
3: pop ebp
 sub ebp,offset 3b
 # Saved original ESP in pushad points below pushfd; argument is +8.
 mov eax,[esp+12]
 push dword ptr [eax+8]
 call 0xf4370
 add esp,4
 test eax,eax
 jz 4f
 mov esi,eax
 lea edx,[ebp+noclip_text]
 call write_log
4: popad
 popfd
 ret
write_log:
 cld
 sub esp,256
 mov ebx,esp
 mov edi,esp
 mov ecx,96
5: mov al,[edx]
 inc edx
 test al,al
 jz 6f
 stosb
 loop 5b
6: lea edx,[esi+0x1e8]
 mov ecx,32
7: mov al,[edx]
 inc edx
 test al,al
 jz 8f
 cmp al,32
 jae 9f
 mov al,'?'
9: stosb
 loop 7b
8: mov al,13
 stosb
 mov al,10
 stosb
 push -11
 call dword ptr [ebp+0x1170cc]
 test eax,eax
 jz 10f
 cmp eax,-1
 je 10f
 mov ecx,edi
 sub ecx,ebx
 lea edx,[ebx+240]
 push 0
 push edx
 push ecx
 push ebx
 push eax
 call dword ptr [ebp+0x117098]
10: add esp,256
 ret
inv_on: .asciz "[KPCFix Server Tools] Invulnerability ON for "
inv_off: .asciz "[KPCFix Server Tools] Invulnerability OFF for "
noclip_text: .asciz "[KPCFix Server Tools] Noclip toggle accepted for "
