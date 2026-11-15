#include "winmini.h"
typedef struct { I32 left,top,right,bottom; } Rect;
typedef U32 (WINAPI *Blt)(void *,const Rect *,void *,const Rect *,U32,void *);
void *g_movie_tail;
/* Integer centred fit, including narrow displays. */
U32 CDECL kpc_fit43(U32 width,U32 height,Rect *out){
    U32 w=width,h=height;
    if(!out||!w||!h||w>16384||h>16384)return 0;
    if(w*3>h*4)w=h*4/3;else h=w*3/4;
    if(!w||!h)return 0;
    out->left=(I32)((width-w)/2);out->top=(I32)((height-h)/2);
    out->right=out->left+(I32)w;out->bottom=out->top+(I32)h;
    return 1;
}

/* Optional GDI halftone stretch for the movie alone. DirectDraw retains its
 * configured blit filter for the cursor, HUD, and other game surfaces. */
U32 g_movie_filter;
typedef I32 (WINAPI *SurfaceDC)(void *,void **);
typedef I32 (WINAPI *ReleaseDCFn)(void *,void *);
typedef I32 (WINAPI *StretchFn)(void *,int,int,int,int,void *,int,int,int,int,U32);
typedef int (WINAPI *ModeFn)(void *,int);
typedef int (WINAPI *OriginFn)(void *,int,int,I32 *);
static StretchFn stretch;
static ModeFn set_mode;
static OriginFn set_origin;
static int initialized;

static int movie_stretch(void *dest,const Rect *dst,void *src,const Rect *r){
    if(!g_movie_filter || !r || r->right<=r->left || r->bottom<=r->top || dest==src)return 0;
    if(!initialized){
        HMODULE gdi=GetModuleHandleW(L"gdi32.dll");
        if(gdi){
            stretch=(StretchFn)GetProcAddress(gdi,"StretchBlt");
            set_mode=(ModeFn)GetProcAddress(gdi,"SetStretchBltMode");
            set_origin=(OriginFn)GetProcAddress(gdi,"SetBrushOrgEx");
        }
        initialized=1;
    }
    if(!stretch || !set_mode || !set_origin)return 0;
    void *in=0,*out=0;
    U32 *src_ops=*(U32 **)src,*dst_ops=*(U32 **)dest;
    if(((SurfaceDC)src_ops[17])(src,&in)!=0)return 0;
    int ok=0;
    if(((SurfaceDC)dst_ops[17])(dest,&out)==0){
        int old=set_mode(out,4); /* HALFTONE */
        if(old){
            I32 origin[2];
            if(set_origin(out,0,0,origin)){
                ok=stretch(out,dst->left,dst->top,dst->right-dst->left,dst->bottom-dst->top,
                    in,r->left,r->top,r->right-r->left,r->bottom-r->top,0x00cc0020); /* SRCCOPY */
                set_origin(out,origin[0],origin[1],0);
            }
            set_mode(out,old);
        }
        if(((ReleaseDCFn)dst_ops[26])(dest,out)!=0)ok=0;
    }
    if(((ReleaseDCFn)src_ops[26])(src,in)!=0)ok=0;
    return ok;
}

/* Only the full-screen Smacker stretch branch calls this wrapper. */
U32 WINAPI kpc_movie_blt(void *dest,const Rect *rect,void *source,
        const Rect *source_rect,U32 flags,void *effects){
    Blt original=(Blt)(*(U32 **)dest)[0x14/4];Rect fit,absolute;U32 fx[25]={0};
    if(rect&&source&&rect->right>rect->left&&rect->bottom>rect->top&&
       kpc_fit43((U32)(rect->right-rect->left),(U32)(rect->bottom-rect->top),&fit)){
        absolute=fit;absolute.left+=rect->left;absolute.right+=rect->left;
        absolute.top+=rect->top;absolute.bottom+=rect->top;
        if(absolute.left!=rect->left||absolute.top!=rect->top||absolute.right!=rect->right||absolute.bottom!=rect->bottom){
            fx[0]=sizeof(fx); /* DDBLTFX, dwFillColor at +80 is zero. */
            if(original(dest,rect,0,0,0x01000400u,fx))
                return original(dest,rect,source,source_rect,flags,effects);
        }
        if(!effects && movie_stretch(dest,&absolute,source,source_rect))return 0;
        return original(dest,&absolute,source,source_rect,flags,effects);
    }
    return original(dest,rect,source,source_rect,flags,effects);
}
