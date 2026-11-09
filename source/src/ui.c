/* Reviewed original-binary UI paths only. No DirectInput-device interception. */
#include "ui.h"
#include "hud.h"
#include "shell_layout.h"
#define THISCALL __attribute__((thiscall))
U32 g_cursor_percent=100,g_lock_input_rate,g_hide_input_rate;
U32 g_tooltip_percent=100;
U8 *g_ui_shell;
void *g_rate_setter_tail;
static U32 hidden_slider_vtable[16];

static int is_rate(const char *s){
    const char *name="inputrate";U32 i;
    if(!s)return 0;
    for(i=0;name[i];i++){
        U8 a=(U8)s[i];if(a>='A'&&a<='Z')a+=32;
        if(a!=(U8)name[i])return 0;
    }
    return !s[i];
}

/* Both calls to the console's original low-level value setter pass here.
 * Its string, float and bound integer therefore agree, including later saves. */
U32 CDECL kpc_rate_value(void *context,void *variable,const char *value){
    typedef U32 (CDECL *Fn)(void *,void *,const char *);
    if(g_lock_input_rate&&variable&&is_rate(*(const char **)((U8 *)variable+0x10)))value="0";
    return ((Fn)0x01cba870u)(context,variable,value);
}

/* Keep the original bitmap pixels and handles. Only this cursor-loader query
 * reports the enlarged bounds used by its original background allocator. */
void CDECL kpc_cursor_dims(U32 *api,U32 surface,U32 *width,U32 *height){
    typedef void (CDECL *Dims)(U32,U32 *,U32 *);
    ((Dims)api[0x174/4])(surface,width,height);
    if(width&&height&&*width&&*height&&*width<=512&&*height<=512){
        *width=(*width*g_cursor_percent+50)/100;
        *height=(*height*g_cursor_percent+50)/100;
        if(!*width)*width=1;if(!*height)*height=1;
    }
}

/* Draw from the complete ORIGINAL source rectangle into the enlarged screen
 * rectangle. No offscreen cursor stretch, replacement bitmap or input change.
 * Original save/restore uses the enlarged loader bounds reported above. */
U32 CDECL kpc_cursor_draw(U32 *api,U8 *cursor,U32 dest,U32 source,
        const void *src_rect,I32 x,I32 y,U32 color){
    typedef U32 (CDECL *Draw)(U32,U32,const void *,I32,I32,U32);
    typedef U32 (CDECL *Scale)(U32,U32,const void *,const void *,U32);
    struct {I32 left,top,right,bottom;} rectangle;
    U32 frame=*(U32 *)(cursor+0x24),width,height;
    if(!src_rect&&g_cursor_percent!=100&&frame<2){
        width=*(U32 *)(cursor+0x14+frame*4);height=*(U32 *)(cursor+0x1c+frame*4);
        if(width&&height&&width<=2048&&height<=2048&&x>=-32768&&x<=32768&&y>=-32768&&y<=32768){
            rectangle.left=x;rectangle.top=y;rectangle.right=x+(I32)width;rectangle.bottom=y+(I32)height;
            U32 result=((Scale)api[0x190/4])(dest,source,&rectangle,NULL,color);
            if(!result)return result;
        }
    }
    return ((Draw)api[0x188/4])(dest,source,src_rect,x,y,color);
}

/* The native tooltip owns a separate Arial font. Create it at the requested
 * size so the existing string metrics and tooltip-buffer allocation agree. */
U32 CDECL kpc_tooltip_font(U32 *api,const char *name,U32 width,U32 height,U32 a,U32 b,U32 c){
    typedef U32 (CDECL *Font)(const char *,U32,U32,U32,U32,U32);
    if(width<=512&&height<=512){
        width=(width*g_tooltip_percent+50)/100;height=(height*g_tooltip_percent+50)/100;
        if(!width)width=1;if(!height)height=1;
    }
    return ((Font)api[0x124/4])(name,width,height,a,b,c);
}

static void THISCALL hidden_draw(void *self,U32 screen){(void)self;(void)screen;}
static U32 THISCALL hidden_event(void *self,U32 a,U32 b){(void)self;(void)a;(void)b;return 0;}

/* Keep the seven original widget indices: mouse-menu dispatch contains fixed
 * index tables for sliders, checkboxes and wheel lists. Only the obsolete
 * widget gets a private vtable. Its original cleanup and destructor survive. */
U32 THISCALL kpc_mouse_menu_ready(void *menu){
    typedef U32 (THISCALL *Fn)(void *);U32 i,*widget,*original,result;
    result=((Fn)(g_ui_shell+g_shell_layout->menu_ready))(menu);
    widget=*(U32 **)((U8 *)menu+0x68);
    if(!widget)return result;
    original=(U32 *)widget[0];
    for(i=0;i<16;i++)hidden_slider_vtable[i]=original[i];
    hidden_slider_vtable[0x0c/4]=(U32)hidden_draw;
    hidden_slider_vtable[0x24/4]=(U32)hidden_event;
    hidden_slider_vtable[0x28/4]=(U32)hidden_event;
    hidden_slider_vtable[0x2c/4]=(U32)hidden_event;
    hidden_slider_vtable[0x34/4]=(U32)hidden_draw;
    hidden_slider_vtable[0x38/4]=(U32)hidden_event;
    widget[0]=(U32)hidden_slider_vtable;
    ((U8 *)widget)[0x24]=0;((U8 *)widget)[0x26]=1;
    return result;
}

U32 THISCALL kpc_mouse_menu_key(void *menu,U32 key,U32 argument){
    typedef U32 (THISCALL *Fn)(void *,U32,U32);U32 result;
    result=((Fn)(g_ui_shell+g_shell_layout->menu_key))(menu,key,argument);
    if((key==0x26||key==0x28)&&*(U32 *)((U8 *)menu+0x88)==1)
        result=((Fn)(g_ui_shell+g_shell_layout->menu_key))(menu,key,argument);
    return result;
}
