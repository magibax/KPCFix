#include "winmini.h"
#include "console.h"

/* Console toggle key character ([KPCFix] ConsoleKeyFix, default 1).
 *
 * client.exe 16131ccf (fixed base 0x1ca0000), window procedure 0x6be0:
 *  - WM_KEYDOWN (0x6db6): virtual key 0xC0 (VK_OEM_3) toggles the console
 *    (0x6dc9 calls 0x287a0, which returns [0x1d4a60c]). VK_OEM_3 is the
 *    backtick on US layouts but the apostrophe on UK layouts.
 *  - WM_CHAR (0x6ffa): while the console is open ([0x1d4a608], 0x28760) every
 *    character except 0x60 (backtick, 0x700a) is appended to the input line.
 * So on a UK keyboard the apostrophe that follows the toggle key-down is
 * typed into the console. The key-down is timestamped; the next character
 * within CONSOLE_KEY_WINDOW_MS is dropped, whatever the layout produces. The
 * backtick test is kept. */
U32 g_console_key_fix;

static U32 key_ms,key_pending;
#define CONSOLE_KEY_WINDOW_MS 250u

U32 CDECL kpc_console_key_down(void){
    key_ms=GetTickCount();key_pending=1;
    return *(volatile U32 *)0x01d4a60cu;
}
U32 CDECL kpc_console_char_drop(void){
    U32 drop=key_pending&&(U32)(GetTickCount()-key_ms)<CONSOLE_KEY_WINDOW_MS;
    key_pending=0;
    return drop;
}
/* Replaces "call 0x287a0" at 0x6dc9 (console key down). */
void kpc_console_key_entry(void);
__asm__(".global kpc_console_key_entry\n"
        "kpc_console_key_entry:\n"
        " pushl %ecx\n"
        " pushl %edx\n"
        " call kpc_console_key_down\n"
        " popl %edx\n"
        " popl %ecx\n"
        " ret\n");
/* Replaces "cmp eax,0x60 / je 0x7054" at 0x700a (WM_CHAR, console open):
 * EAX = character. Skips to 0x7054 for a backtick or a dropped character,
 * otherwise continues at 0x700f. */
void kpc_console_char_entry(void);
__asm__(".global kpc_console_char_entry\n"
        "kpc_console_char_entry:\n"
        " cmpl $0x60,%eax\n"
        " je 2f\n"
        " pushl %eax\n"
        " pushl %ecx\n"
        " pushl %edx\n"
        " call kpc_console_char_drop\n"
        " testl %eax,%eax\n"
        " popl %edx\n"
        " popl %ecx\n"
        " popl %eax\n"
        " jnz 2f\n"
        " ret\n"
        "2:\n"
        " addl $4,%esp\n"
        " pushl $0x01ca7054\n"
        " ret\n");
