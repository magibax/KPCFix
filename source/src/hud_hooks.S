.intel_syntax noprefix
.section .text
.extern kpc_hud_panel_draw,kpc_hud_panel_post,kpc_hud_adjust_rect
.extern kpc_hud_solid,kpc_hud_scale,kpc_hud_cross
.extern kpc_hud_carousel

.global hud_panel_entry
hud_panel_entry:
 push DWORD PTR [esp+4]
 call kpc_hud_panel_draw
 mov edx,DWORD PTR [esi]   # overwritten original post-call instruction
 ret 4
.global hud_post_entry
hud_post_entry:
 # Called before the original argument push. EAX already holds frame bits.
 # Keep 0x9FA18 (a shared branch destination) completely intact.
 push eax
 call kpc_hud_panel_post
 ret

.global hud_rect_entry
hud_rect_entry:
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 push DWORD PTR [ebp+40]
 push DWORD PTR [ebp+24]
 call kpc_hud_adjust_rect
 add esp,8
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 ret 4

# Original drawing calls are cdecl. Keep all original arguments for caller cleanup.
.global hud_solid_ecx
hud_solid_ecx:
 push ecx
 mov edx,esp
 push DWORD PTR [edx+28]
 push DWORD PTR [edx+24]
 push DWORD PTR [edx+20]
 push DWORD PTR [edx+16]
 push DWORD PTR [edx+12]
 push DWORD PTR [edx+8]
 push DWORD PTR [edx]
 call kpc_hud_solid
 add esp,28
 add esp,4
 ret

.macro scale_bridge name,api
.global \name
\name:
 push \api
 mov edx,esp
 push DWORD PTR [edx+24]
 push DWORD PTR [edx+20]
 push DWORD PTR [edx+16]
 push DWORD PTR [edx+12]
 push DWORD PTR [edx+8]
 push DWORD PTR [edx]
 call kpc_hud_scale
 add esp,24
 add esp,4
 ret
.endm
scale_bridge hud_scale_eax,eax
scale_bridge hud_scale_edx,edx

.macro cross_bridge name,api
.global \name
\name:
 push \api
 mov edx,esp
 push DWORD PTR [edx+28]
 push DWORD PTR [edx+24]
 push DWORD PTR [edx+20]
 push DWORD PTR [edx+16]
 push DWORD PTR [edx+12]
 push DWORD PTR [edx+8]
 push ebp
 push DWORD PTR [edx]
 call kpc_hud_cross
 add esp,32
 add esp,4
 ret
.endm
cross_bridge hud_cross_ecx,ecx
cross_bridge hud_cross_eax,eax

.global hud_carousel_entry
hud_carousel_entry:
 mov edx,esp
 push DWORD PTR [edx+20]
 push DWORD PTR [edx+16]
 push DWORD PTR [edx+12]
 push DWORD PTR [edx+8]
 push DWORD PTR [edx+4]
 call kpc_hud_carousel
 ret 20
.section .note.GNU-stack,"",@progbits
