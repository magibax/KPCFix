.intel_syntax noprefix
.section .text
.extern kpc_cursor_dims,kpc_cursor_draw,kpc_tooltip_font,g_rate_setter_tail
.global ui_cursor_dims_entry
ui_cursor_dims_entry:
 mov eax,esp
 push DWORD PTR [eax+12]
 push DWORD PTR [eax+8]
 push DWORD PTR [eax+4]
 push edx
 call kpc_cursor_dims
 add esp,16
 ret
.global ui_cursor_draw_entry
ui_cursor_draw_entry:
 mov eax,esp
 push DWORD PTR [eax+24]
 push DWORD PTR [eax+20]
 push DWORD PTR [eax+16]
 push DWORD PTR [eax+12]
 push DWORD PTR [eax+8]
 push DWORD PTR [eax+4]
 push esi
 push edx
 call kpc_cursor_draw
 add esp,32
 ret
.global ui_tooltip_font_entry
ui_tooltip_font_entry:
 mov edx,esp
 push DWORD PTR [edx+24]
 push DWORD PTR [edx+20]
 push DWORD PTR [edx+16]
 push DWORD PTR [edx+12]
 push DWORD PTR [edx+8]
 push DWORD PTR [edx+4]
 push eax
 call kpc_tooltip_font
 add esp,28
 ret
.macro profile_setter name,frame
.global \name
\name:
 cmp DWORD PTR [esp+4],0
 jne 1f
 cmp DWORD PTR [esp+8],3
 jne 1f
 mov DWORD PTR [esp+12],0 # InputRate's category/index only
1:
 sub esp,\frame            # overwritten complete original instruction
 jmp DWORD PTR g_rate_setter_tail
 .endm
profile_setter ui_rate_setter_entry,0x90
profile_setter ui_rate_setter_italian,0x94
.section .note.GNU-stack,"",@progbits
