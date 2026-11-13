.intel_syntax noprefix
.code32
.global inv_hook,noclip_hook,chat_hook,flags_hook
# Runtime policy lives in newly reserved zero-initialized .data padding.
.equ STATE,0x174f00
.equ PENDING,0x174f10
.global init_hook,inv_gate,noclip_request,leave_hook,host_command,initial_mode
init_hook:
 pushfd
 pushad
 call 81f
81: pop ebp
 sub ebp,offset 81b
 movzx eax,byte ptr [ebp+initial_mode]
 mov [ebp+STATE],eax
 mov dword ptr [ebp+STATE+4],0
 cld
 lea edi,[ebp+PENDING]
 xor eax,eax
 mov ecx,32
 rep stosd
 call find_console
 test esi,esi
 jz init_done
 push 0
 call dword ptr [ebp+0x117058]
 mov ebx,eax
 lea ecx,[ebx+0x10e50]
 lea edi,[ebx+0x1640]
 lea edx,[ebx+0x15f0]
 lea eax,[ebx+0x12f70]
 cmp esi,eax
 je init_register
 lea ecx,[ebx+0x13f00]
 lea edi,[ebx+0x16e0]
 lea edx,[ebx+0x1690]
init_register:
 # ECX=command table, EDX=add(name, fn), EDI=find(name), both thiscall.
 push ecx
 push edx
 push edi
 lea eax,[ebp+command_name]
 lea esi,[ebp+host_command]
 call register_one
 test eax,eax
 jz init_level
 mov dword ptr [ebp+STATE+4],1
init_level:
 # Co-op: "changelevel <world>" forwards to the C callback at STATE+8.
 lea eax,[ebp+level_name]
 lea esi,[ebp+level_command]
 call register_one
 add esp,12
init_done:
 mov eax,[ebp+0x16de60]
 mov [esp+28],eax
 popad
 popfd
 ret
# EAX=name, ESI=callback; [esp+4]=find, [esp+8]=add, [esp+12]=table.
# Find first: adding an existing list node would create a cycle (an existing
# node gets the callback). Returns EAX non-zero when registered.
register_one:
 push ebx
 mov ebx,eax
 mov ecx,[esp+16]
 push ebx
 call dword ptr [esp+12]
 test eax,eax
 jz 92f
 mov [eax],esi
 jmp 93f
92: mov ecx,[esp+16]
 push esi
 push ebx
 call dword ptr [esp+20]
93: pop ebx
 ret
# Dedicated console "changelevel": argument block as kpc_cheats (count, words).
level_command:
 pushfd
 pushad
 call 94f
94: pop ebp
 sub ebp,offset 94b
 mov eax,[esp+12]
 mov esi,[eax+8]
 mov eax,[ebp+STATE+8]
 test eax,eax
 jz 95f
 push esi
 call eax
 add esp,4
 jmp 96f
95: lea edx,[ebp+level_off]
 call print_cstring
96: popad
 popfd
 ret
# Produce the flags consumed by the original JNE multiplayer rejection.
inv_gate:
 push ecx
 call 82f
82: pop eax
 sub eax,offset 82b
 cmp dword ptr [eax+STATE+4],1
 jne inv_stock
 cmp dword ptr [eax+STATE],1
 pop ecx
 ret
inv_stock:
 mov eax,[ecx+0x1d0]
 test eax,eax
 pop ecx
 ret
noclip_request:
 push eax
 call 83f
83: pop eax
 sub eax,offset 83b
 cmp dword ptr [eax+STATE+4],1
 jne noclip_allow
 cmp dword ptr [eax+STATE],1
 jne noclip_deny
noclip_allow:
 pop eax
 mov eax,[esp+4]
 push 44
 push eax
 call 0xf96f0
 add esp,8
 ret
noclip_deny:
 pop eax
 ret
# The callback is registered only in the dedicated app's host command table.
host_command:
 pushfd
 pushad
 call 84f
84: pop ebp
 sub ebp,offset 84b
 mov eax,[esp+12]
 mov esi,[eax+8]
 cmp dword ptr [esi],1
 je host_status
 cmp dword ptr [esi],2
 jne host_usage
 mov eax,[esi+8]
 cmp byte ptr [eax+1],0
 jne host_usage
 movzx eax,byte ptr [eax]
 sub eax,'0'
 cmp eax,1
 ja host_usage
 cmp eax,[ebp+STATE]
 je host_status
 mov [ebp+STATE],eax
 test eax,eax
 jnz host_announce
 call revoke_players
host_announce:
 lea edx,[ebp+policy_off]
 cmp dword ptr [ebp+STATE],0
 je host_send
 lea edx,[ebp+policy_on]
host_send:
 push edx
 add edx,22
 call broadcast_only
 pop edx
 call print_cstring
 jmp host_done
host_status:
 lea edx,[ebp+policy_off]
 cmp dword ptr [ebp+STATE],0
 je host_print
 lea edx,[ebp+policy_on]
host_print:
 call print_cstring
 jmp host_done
host_usage:
 lea edx,[ebp+policy_usage]
 call print_cstring
host_done:
 popad
 popfd
 ret
print_cstring:
 xor ecx,ecx
85: cmp byte ptr [edx+ecx],0
 je 86f
 inc ecx
 jmp 85b
86: call console_line
 ret
# EDX=client handle. Free its pending OFF entry on acknowledgement/disconnect.
clear_pending:
 lea ecx,[ebp+PENDING]
 mov eax,16
87: cmp [ecx],edx
 jne 88f
 mov dword ptr [ecx],0
 mov dword ptr [ecx+4],0
 ret
88: add ecx,8
 dec eax
 jnz 87b
 ret
# ESI=player; send at most one correction until OFF is acknowledged.
force_noclip_off:
 pushad
 mov edx,[esi+0x120]
 test edx,edx
 jz force_done
 lea ecx,[ebp+PENDING]
 xor ebx,ebx
 mov eax,16
89: cmp [ecx],edx
 je force_done
 cmp dword ptr [ecx],0
 jne 90f
 test ebx,ebx
 jnz 90f
 mov ebx,ecx
90: add ecx,8
 dec eax
 jnz 89b
 test ebx,ebx
 jz force_done
 mov [ebx],edx
 mov dword ptr [ebx+4],1
 push 44
 push edx
 call 0xf96f0
 add esp,8
force_done:
 popad
 ret
revoke_players:
 mov ecx,[ebp+0x157724]
 test ecx,ecx
 jz revoke_done
 push 0
 call 0x7d220
 mov ebx,eax
revoke_next:
 test ebx,ebx
 jz revoke_done
 # List values are HOBJECTs, not player pointers. Match f4370's lookup.
 push ebx
 mov eax,esp
 push eax
 call 0xee7d0
 add esp,8
 test eax,eax
 jz revoke_advance
 mov esi,eax
 mov byte ptr [esi+0x13f],0
 push dword ptr [esi+8]
 mov eax,[ebp+0x16de80]
 call dword ptr [eax+0x1f8]
 add esp,4
 test eax,0x200000
 jz revoke_advance
 # Request the normal client OFF transition. Do not make the server solid
 # while the client still owns a flying/noclip movement state.
 call force_noclip_off
revoke_advance:
 mov ecx,[ebp+0x157724]
 push 0
 push ebx
 call 0x7d260
 mov ebx,eax
 jmp revoke_next
revoke_done:
 ret
leave_hook:
 pushfd
 pushad
 call 91f
91: pop ebp
 sub ebp,offset 91b
 mov eax,[esp+12]
 mov edx,[eax+12]
 call clear_pending
 popad
 popfd
 mov ecx,[esp+4]
 test ecx,ecx
 jmp 0xfd4f6
command_name: .asciz "kpc_cheats"
policy_on: .asciz "[KPCFix Server Tools] Cheats enabled"
policy_off: .asciz "[KPCFix Server Tools] Cheats disabled"
policy_usage: .asciz "[KPCFix Server Tools] Usage: kpc_cheats 0 or kpc_cheats 1; kpc_cheats shows status"
level_name: .asciz "changelevel"
level_off: .asciz "[KPCFix Server Tools] changelevel needs the co-op campaign ([Coop] CoopSession=1, CoopCampaign=1)"
initial_mode: .byte 1

# All entry hooks preserve the caller's flags and registers.
inv_hook:
 pushfd
 pushad
 call 1f
1: pop ebp
 sub ebp,offset 1b
 lea edx,[ebp+inv_off]
 cmp byte ptr [esi+0x13f],0
 je 2f
 lea edx,[ebp+inv_on]
2: call cheat_line
 popad
 popfd
 # Replace (do not invoke) the original unprefixed diagnostic call.
 ret
# Compatibility symbol; the request callsite is restored to the stock target.
noclip_hook:
 jmp 0xf96f0
# MID_PLAYER_FLAGS applies the collision state reported by the client.
# Announce only a change of its noclip bit; preserve the original flag write.
flags_hook:
 pushfd
 pushad
 call 3f
3: pop ebp
 sub ebp,offset 3b
 mov eax,[esp+12]
 mov edi,[eax+12]
 test edi,0x200000
 jnz flags_policy
 mov edx,[esi+0x120]
 call clear_pending
flags_policy:
 cmp dword ptr [ebp+STATE+4],1
 jne flags_read
 cmp dword ptr [ebp+STATE],0
 jne flags_read
 test edi,0x200000
 jz flags_read
 call force_noclip_off
 # Keep existing collision/noclip bits until the client's OFF reply arrives.
 push dword ptr [esi+8]
 mov eax,[ebp+0x16de80]
 call dword ptr [eax+0x1f8]
 add esp,4
 and edi,0xffdfdfff
 and eax,0x202000
 or edi,eax
flags_read:
 push dword ptr [esi+8]
 mov eax,[ebp+0x16de80]
 call dword ptr [eax+0x1f8]
 add esp,4
 xor eax,edi
 and eax,0x200000
 push eax
 push edi
 push dword ptr [esi+8]
 mov eax,[ebp+0x16de80]
 call dword ptr [eax+0x1fc]
 add esp,8
 pop eax
 cmp dword ptr [ebp+STATE+4],1
 jne flags_notify
 cmp dword ptr [ebp+STATE],0
 je 4f
flags_notify:
 test eax,eax
 jz 4f
 lea edx,[ebp+noclip_off]
 test edi,0x200000
 jz 6f
 lea edx,[ebp+noclip_on]
6: call cheat_line
4: popad
 popfd
 ret
# MID_MULTIMSG callsite: (recipient, flags, format, player name, text).
# Mirror once; tail-call the original function with every argument intact.
chat_hook:
 pushfd
 pushad
 call 5f
5: pop ebp
 sub ebp,offset 5b
 cld
 mov eax,[esp+12]
 mov esi,[eax+20]
 mov edx,[eax+24]
 sub esp,512
 mov ebx,esp
 mov edi,esp
 push edx
 lea edx,[ebp+chat_prefix]
 mov ecx,64
 call copy_safe
 mov edx,esi
 mov ecx,32
 call copy_safe
 mov al,':'
 stosb
 mov al,' '
 stosb
 pop edx
 mov ecx,320
 call copy_safe
 mov ecx,edi
 sub ecx,ebx
 mov edx,ebx
 call console_line
 add esp,512
 popad
 popfd
 jmp 0xf8e90
# ESI=player, EDX=action suffix. Console prefix is excluded from the broadcast.
cheat_line:
 cld
 sub esp,256
 mov ebx,esp
 mov edi,esp
 push edx
 lea edx,[ebp+server_prefix]
 mov ecx,64
 call copy_safe
 mov [ebx+248],edi
 lea edx,[esi+0x1e8]
 mov ecx,32
 call copy_safe
 pop edx
 mov ecx,96
 call copy_safe
 mov byte ptr [edi],0
 # Send the same packet directly, bypassing SAY's optional console echo.
 mov edx,[ebx+248]
 call broadcast_only
 mov ecx,edi
 sub ecx,ebx
 mov edx,ebx
 call console_line
 add esp,256
 ret
# EDX=text. Flags=4 retain the chat entry and category word, clearing
# flag 1 (the centre-screen copy). No formatter
# or shell-controlled console echo. Preserve the caller's buffer registers.
broadcast_only:
 push ebx
 push esi
 push edi
 push edx
 call 0xf88c0
 mov esi,eax
 mov ecx,esi
 mov eax,[esi]
 push 4
 call dword ptr [eax+0x3c]
 mov ecx,esi
 mov eax,[esi]
 push 0
 call dword ptr [eax+0x3c]
 mov edx,[esp]
 mov ecx,esi
 mov eax,[esi]
 push edx
 call dword ptr [eax+0x4c]
 mov ecx,[ebp+0x16de80]
 mov eax,[ecx]
 push 1
 push 0
 push 0x45
 push esi
 call dword ptr [eax+0xc4]
 mov ecx,esi
 mov eax,[esi]
 call dword ptr [eax]
 add esp,4
 pop edi
 pop esi
 pop ebx
 ret
# Bounded byte copy, flatten console controls, leave high ANSI bytes intact.
copy_safe:
 test edx,edx
 jz copy_done
copy_next:
 mov al,[edx]
 inc edx
 test al,al
 jz copy_done
 cmp al,32
 jb copy_replace
 cmp al,127
 jne copy_store
copy_replace:
 mov al,'?'
copy_store:
 stosb
 loop copy_next
copy_done:
 ret
# EDX=text, ECX=length. EBX/ESI/EDI/EBP preserved by this routine.
# Native console editing state is read only. Runs in the server event thread.
console_line:
 push ebx
 push esi
 push edi
 sub esp,128
 mov ebx,esp
 mov [ebx+0],edx
 mov [ebx+4],ecx
 mov dword ptr [ebx+16],0
 push -11
 call dword ptr [ebp+0x1170cc]
 mov [ebx+8],eax
 test eax,eax
 jz console_done
 cmp eax,-1
 je console_done
 call find_console
 test esi,esi
 jz plain_line
 # Console mode distinguishes interactive console from redirected log files.
 lea eax,[ebp+kernel_name]
 push eax
 call dword ptr [ebp+0x117058]
 test eax,eax
 jz plain_line
 lea edx,[ebp+mode_name]
 push edx
 push eax
 call dword ptr [ebp+0x117054]
 test eax,eax
 jz plain_line
 lea edx,[ebx+20]
 push edx
 push dword ptr [ebx+8]
 call eax
 test eax,eax
 jz plain_line
 # Stock editor: buffer[79], cursor, length, prompt[79]. Validate bounds.
 mov eax,[esi+0x5c]
 cmp eax,78
 ja plain_line
 mov ecx,[esi+0x58]
 cmp ecx,eax
 ja plain_line
 cmp byte ptr [esi+0x64],'>'
 jne plain_line
 cmp byte ptr [esi+0x65],0
 jne plain_line
 mov [ebx+16],esi
 # Erase the old prompt/input from column zero, including text after cursor.
 lea edi,[ebx+32]
 mov byte ptr [edi],13
 inc edi
 mov ecx,[esi+0x5c]
 inc ecx
 mov al,' '
 rep stosb
 mov byte ptr [edi],13
 inc edi
 lea edx,[ebx+32]
 mov ecx,edi
 sub ecx,edx
 call raw_write
plain_line:
 mov edx,[ebx+0]
 mov ecx,[ebx+4]
 call raw_write
 lea edx,[ebp+newline]
 mov ecx,2
 call raw_write
 mov esi,[ebx+16]
 test esi,esi
 jz console_done
 lea edx,[esi+0x64]
 mov ecx,1
 call raw_write
 lea edx,[esi+4]
 mov ecx,[esi+0x5c]
 call raw_write
 # Restore insertion cursor, including editing in the middle of a command.
 mov ecx,[esi+0x5c]
 sub ecx,[esi+0x58]
 lea edi,[ebx+32]
 mov edx,edi
 mov al,8
 push ecx
 rep stosb
 pop ecx
 call raw_write
console_done:
 add esp,128
 pop edi
 pop esi
 pop ebx
 ret
raw_write:
 test ecx,ecx
 jz raw_done
 lea eax,[ebx+12]
 push 0
 push eax
 push ecx
 push edx
 push dword ptr [ebx+8]
 call dword ptr [ebp+0x117098]
raw_done:
 ret
# Recognize both supplied dedicated builds before reading their editor state.
# No writes or hooks in dedicated.exe, and no hard-coded addresses in clients.
find_console:
 xor esi,esi
 push 0
 call dword ptr [ebp+0x117058]
 test eax,eax
 jz find_done
 cmp word ptr [eax],0x5a4d
 jne find_done
 mov edx,[eax+0x3c]
 cmp edx,0x1000
 ja find_done
 cmp dword ptr [eax+edx],0x4550
 jne find_done
 cmp dword ptr [eax+edx+8],0x39b907ab
 je europe_console
 cmp dword ptr [eax+edx+8],0x3a942fb8
 jne find_done
 cmp dword ptr [eax+edx+0x50],0x1b000
 jne find_done
 lea edi,[eax+0x1de3]
 lea ecx,[eax+0x16028]
 jmp check_console
 europe_console:
 cmp dword ptr [eax+edx+0x50],0x18000
 jne find_done
 lea edi,[eax+0x1c5a]
 lea ecx,[eax+0x12f70]
check_console:
 cmp byte ptr [edi],0xb9
 jne find_done
 cmp [edi+1],ecx
 jne find_done
 cmp byte ptr [edi+5],0xe8
 jne find_done
 mov esi,ecx
find_done:
 ret
server_prefix: .asciz "[KPCFix Server Tools] "
inv_on: .asciz " toggled invulnerability ON"
inv_off: .asciz " toggled invulnerability OFF"
noclip_on: .asciz " toggled noclip ON"
noclip_off: .asciz " toggled noclip OFF"
chat_prefix: .asciz "[KPCFix Server Tools] Chat: "
newline: .ascii "\r\n"
kernel_name: .asciz "kernel32.dll"
mode_name: .asciz "GetConsoleMode"
