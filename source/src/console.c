#include "winmini.h"
#include "console.h"
#include "shell_layout.h"

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

/* Chat typing ([Multiplayer] ChatBlocksActions, default 1).
 * cshell 703813a4: key bindings reach the shell as actions through
 * OnCommandOn (vtable slot 0xff724 -> 0xc9d80, thiscall, int action, ret 4),
 * including developer actions such as NoClip (90) and Invuln (93) that send
 * Server Tools requests, while the multiplayer message editor at shell+0x8c is
 * open (its typing byte +0x12ed, read by 0xd85f0, set by 0xd8600). Typed
 * characters reach the editor through key events, not actions. While the
 * editor is typing, every action is dropped, so letters typed in a message
 * no longer toggle noclip, move or fire. */
U32 g_chat_blocks_actions;
U8 *g_command_shell;
static U32 blocked_actions;
typedef void (__attribute__((thiscall)) *CommandFn)(void *,U32);
void __attribute__((thiscall)) kpc_command_on(void *shell,U32 action){
    const ShellLayout *L=g_shell_layout;
    if(g_chat_blocks_actions&&shell&&*((const U8 *)shell+L->chat_editor+L->chat_editing)){blocked_actions++;return;}
    ((CommandFn)(g_command_shell+L->command_on))(shell,action);
}
U32 kpc_blocked_actions(void){return blocked_actions;}

/* The engine also keeps the bound commands' on/off state itself and sends it
 * to the server: object.lto polls IsCommandOn(client, 93/91) (0xef1b4,
 * 0xef204) for Invuln/NoTarget, so the OnCommandOn filter above cannot stop
 * them. client.exe 16131ccf input update 0x23c26: after the input manager
 * fills the new command buffer, 0x23c84 reads the input-enabled flag
 * [cm+0x1014] (SetInputState) and clears the whole buffer when it is 0.
 * While the chat editor is typing that read reports 0, so the frame has no
 * command on: no movement, firing or developer cheat reaches the shell or the
 * server, and held commands are released as on a normal key-up. Typed text
 * still arrives through WM_CHAR/key events. */
static U32 blocked_frames;
static int chat_typing(void){
    const ShellLayout *L=g_shell_layout;U8 *shell;
    if(!g_chat_blocks_actions||!g_command_shell||!L||!L->client_shell_global||!L->chat_editor)return 0;
    shell=*(U8 **)(g_command_shell+L->client_shell_global);
    return shell&&shell[L->chat_editor+L->chat_editing];
}
U32 CDECL kpc_input_enabled(U32 state){
    if(state&&chat_typing()){blocked_frames++;return 0;}
    return state;
}
U32 kpc_blocked_input_frames(void){return blocked_frames;}
/* Replaces "mov ecx,[eax+0x1014]" (6 bytes) at 0x23c84; EAX = cm. */
void kpc_input_state_entry(void);
__asm__(".global kpc_input_state_entry\n"
        "kpc_input_state_entry:\n"
        " pushl %eax\n"
        " pushl %edx\n"
        " pushl 0x1014(%eax)\n"
        " call kpc_input_enabled\n"
        " addl $4,%esp\n"
        " movl %eax,%ecx\n"
        " popl %edx\n"
        " popl %eax\n"
        " ret\n");
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
