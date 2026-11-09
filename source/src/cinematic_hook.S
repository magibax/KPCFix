.intel_syntax noprefix
.section .text
.extern kpc_movie_blt,g_movie_tail
.global movie_aspect_entry
movie_aspect_entry:
 # Replaces push ebp; call [eax+14]; jmp original continuation.
 # Five existing COM arguments are already on the stack.
 push ebp
 call kpc_movie_blt
 jmp DWORD PTR [g_movie_tail]
.section .note.GNU-stack,"",@progbits

