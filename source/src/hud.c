#include "hud.h"
#include "shell_layout.h"

#define THISCALL __attribute__((thiscall))
typedef struct { I32 left,top,right,bottom; } HudRect;
typedef struct HudScope {
    U8 *panel;
    void *screen;
    I32 width,height,anchor_x,anchor_y;
} HudScope;
typedef void (THISCALL *PanelDraw)(void *,float);
typedef void (THISCALL *PanelPosition)(void *,I32 *,I32 *);
typedef void (THISCALL *PanelNumber)(void *,U32,I32,I32,I32,U32);
typedef U32 (CDECL *SolidDraw)(void *,void *,const HudRect *,I32,I32,U32);
typedef U32 (CDECL *ScaleDraw)(void *,void *,const HudRect *,const HudRect *,U32);
typedef void (CDECL *SurfaceDims)(void *,U32 *,U32 *);

U32 g_hud_percent=100;
U8 *g_hud_shell;
static U32 hud_thread;
static HudScope *volatile active_hud;
static U32 get32(const void *p){return *(const U32 *)p;}

static int panel_scope(HudScope *out,U8 *panel){
    U8 *client;U32 flags;I32 x=0,y=0;
    if(!g_hud_shell||!panel)return 0;
    client=(U8 *)get32(panel+0x28);
    if(!client)return 0;
    out->panel=panel;out->screen=(void *)get32(panel+0x30);
    out->width=(I32)get32(client+0x210);out->height=(I32)get32(client+0x20c);
    kpc_hud_observe((U32)out->width,(U32)out->height,(U32)out->screen);
    if(!out->screen||out->width<320||out->width>16384||out->height<200||out->height>16384)return 0;
    flags=get32(panel+0x18);
    if(flags&0x30)((PanelPosition)(g_hud_shell+g_shell_layout->panel_position))(panel,&x,&y);
    out->anchor_x=(flags&0x10)?x:(flags&8)?out->width:(flags&0x40)?out->width/2:0;
    out->anchor_y=(flags&0x20)?y:(flags&2)?out->height:0;
    return 1;
}

static void panel_draw(U8 *panel,float frame,U32 slot){
    HudScope scope,*previous=NULL;U32 tid=GetCurrentThreadId();int activated=0;
    PanelDraw original=(PanelDraw)get32((U8 *)get32(panel)+slot);
    if(!hud_thread)__sync_val_compare_and_swap(&hud_thread,0,tid);
    if(tid==hud_thread){
        previous=active_hud;
        active_hud=g_hud_percent!=100&&panel_scope(&scope,panel)?&scope:NULL;
        activated=1;
    }
    original(panel,frame);
    if(activated)active_hud=previous;
}
void THISCALL kpc_hud_panel_draw(void *panel,float frame){panel_draw(panel,frame,4);}
void THISCALL kpc_hud_panel_post(void *panel,float frame){panel_draw(panel,frame,8);}

static HudScope *scope_for(void *destination){
    HudScope *scope;
    if(!hud_thread||GetCurrentThreadId()!=hud_thread)return NULL;
    scope=active_hud;
    return scope&&scope->screen==destination?scope:NULL;
}

static int coordinate(I32 value,I32 anchor,I32 *out){
    if(value < -65536||value>65536||anchor < -65536||anchor>65536)return 0;
    *out=anchor+((value-anchor)*(I32)g_hud_percent)/100;
    return 1;
}
static int transform(const HudScope *scope,const HudRect *source,HudRect *out){
    return coordinate(source->left,scope->anchor_x,&out->left)&&
        coordinate(source->right,scope->anchor_x,&out->right)&&
        coordinate(source->top,scope->anchor_y,&out->top)&&
        coordinate(source->bottom,scope->anchor_y,&out->bottom);
}
void CDECL kpc_hud_adjust_rect(U8 *panel,HudRect *rectangle){
    HudScope *scope;HudRect result;
    if(!rectangle||g_hud_percent==100||!hud_thread||GetCurrentThreadId()!=hud_thread)return;
    scope=active_hud;
    if(scope&&scope->panel==panel&&transform(scope,rectangle,&result))*rectangle=result;
}

static int source_size(U8 *api,void *surface,const HudRect *source,I32 *width,I32 *height){
    U32 w=0,h=0;
    if(source){
        if(source->left < -16384||source->left>32768||source->top < -16384||source->top>32768||
           source->right < -16384||source->right>32768||source->bottom < -16384||source->bottom>32768)return 0;
        *width=source->right-source->left;*height=source->bottom-source->top;
    }else{
        if(!surface)return 0;
        ((SurfaceDims)get32(api+0x174))(surface,&w,&h);
        *width=(I32)w;*height=(I32)h;
    }
    return *width>0&&*width<=16384&&*height>0&&*height<=16384;
}
static U32 solid_with_scope(U8 *api,HudScope *scope,void *destination,void *source,
        const HudRect *source_rect,I32 x,I32 y,U32 color){
    I32 width,height;HudRect native,result;
    if(scope&&x>=-65536&&x<=49152&&y>=-65536&&y<=49152&&
       source_size(api,source,source_rect,&width,&height)){
        native.left=x;native.top=y;native.right=x+width;native.bottom=y+height;
        if(transform(scope,&native,&result))
            return ((ScaleDraw)get32(api+0x190))(destination,source,&result,source_rect,color);
    }
    return ((SolidDraw)get32(api+0x188))(destination,source,source_rect,x,y,color);
}
U32 CDECL kpc_hud_solid(U8 *api,void *destination,void *source,
        const HudRect *source_rect,I32 x,I32 y,U32 color){
    return solid_with_scope(api,scope_for(destination),destination,source,source_rect,x,y,color);
}
U32 CDECL kpc_hud_scale(U8 *api,void *destination,void *source,
        const HudRect *destination_rect,const HudRect *source_rect,U32 color){
    HudScope *scope=scope_for(destination);HudRect result;
    if(scope&&destination_rect&&transform(scope,destination_rect,&result))destination_rect=&result;
    return ((ScaleDraw)get32(api+0x190))(destination,source,destination_rect,source_rect,color);
}

/* Carousel coordinates have already passed through its scaled 3D camera.
 * At cshell+0xA1977, x is projected X and y is projected Y plus 20 pixels.
 * Anchor the glyph transform to that projected point: scale glyph size,
 * centering, slide offsets and the 20-pixel gap once, retaining its position. */
void THISCALL kpc_hud_carousel(void *panel,U32 font,I32 x,I32 y,I32 value,U32 flags){
    HudScope local,*previous=scope_for((void *)get32((U8 *)panel+0x30));int changed=0;
    if(previous&&previous->panel==panel&&x>=-65536&&x<=65536&&y>=-65516&&y<=65536){
        local=*previous;local.anchor_x=x;local.anchor_y=y-20;
        active_hud=&local;changed=1;
    }
    ((PanelNumber)(g_hud_shell+g_shell_layout->panel_number))(panel,font,x,y,value,flags);
    if(changed)active_hud=previous;
}

U32 CDECL kpc_hud_cross(U8 *api,U8 *object,void *destination,void *source,
        const HudRect *source_rect,I32 x,I32 y,U32 color){
    HudScope scope;U32 flag,fx,fy;
    if(!g_hud_shell||g_hud_percent==100||!object||!destination||
       get32(object+0x1f8)!=(U32)destination)
        return solid_with_scope(api,NULL,destination,source,source_rect,x,y,color);
    scope.panel=NULL;scope.screen=destination;
    scope.width=(I32)get32(object+0x204);scope.height=(I32)get32(object+0x208);
    if(scope.width<320||scope.width>16384||scope.height<200||scope.height>16384)
        return solid_with_scope(api,NULL,destination,source,source_rect,x,y,color);
    scope.anchor_x=scope.width/2;scope.anchor_y=scope.height/2;
    flag=get32(g_hud_shell+g_shell_layout->crosshair_flag)&0x7fffffffu;
    if(flag){
        fx=get32(g_hud_shell+g_shell_layout->crosshair_x)&0x7fffffffu;
        fy=get32(g_hud_shell+g_shell_layout->crosshair_y)&0x7fffffffu;
        if(fx>0x47800000u||fy>0x47800000u) /* finite and within +/-65536 before cast */
            return solid_with_scope(api,NULL,destination,source,source_rect,x,y,color);
        scope.anchor_x=(I32)*(const float *)(g_hud_shell+g_shell_layout->crosshair_x);
        scope.anchor_y=(I32)*(const float *)(g_hud_shell+g_shell_layout->crosshair_y);
    }
    return solid_with_scope(api,&scope,destination,source,source_rect,x,y,color);
}
