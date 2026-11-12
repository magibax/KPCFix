#include "winmini.h"
#include "camera.h"
#include "shell_layout.h"

/* Smooth scripted cameras in multiplayer ([Multiplayer] SmoothCutsceneCamera,
 * default 1).
 *
 * cshell 703813a4: in multiplayer the server drives a scripted camera path by
 * sending message 0x3d, sub-type 5 (handler 0xa7a60 -> 0xa7bb1) with a position
 * and a rotation every server frame; 0xab0f0 passes them to 0xa8f30 (thiscall
 * on the camera object [0x1c49dc], ret 8), which stores the position at
 * +0x10 and the rotation quaternion at +0x1c. The pose therefore changes only
 * when a sample arrives: 62.5 times a second with the co-op server's 16 ms
 * frames, a visible stepping at higher frame rates.
 *
 * The call at 0xab104 is redirected: each sample becomes the target, and every
 * client frame (client update hook, loading.c kpc_client_frame) moves the pose
 * from where it was shown towards it over the measured interval between
 * samples: position linearly, rotation by normalised quaternion
 * interpolation along the shorter arc. This adds about one sample interval
 * of latency. A jump larger than CAMERA_CUT_UNITS (a camera cut) or the first
 * sample after a pause is applied at once; after CAMERA_STALE_MS without a
 * sample nothing is written any more, so the stock camera code owns the pose
 * again (camera_reset, player view). */
U32 g_camera_smooth;
U8 *g_camera_shell;
U32 (WINAPI *g_camera_clock)(void);
static float shown_pos[3],shown_rot[4],from_pos[3],from_rot[4],to_pos[3],to_rot[4];
static U32 active,sample_ms,interval_ms=16,samples,snaps;
#define CAMERA_CUT_UNITS 256.0f
#define CAMERA_STALE_MS 100u
#define CAMERA_MIN_INTERVAL_MS 4u
#define CAMERA_MAX_INTERVAL_MS 100u

typedef void (__attribute__((thiscall)) *CameraSetFn)(void *,const float *,const float *);

static float square_root(float v){
    float r;
    if(v<=0.0f)return 0.0f;
    __asm__("fsqrt":"=t"(r):"0"(v));
    return r;
}
static U32 now_ms(void){return g_camera_clock?g_camera_clock():GetTickCount();}
static void *camera_object(void){
    const ShellLayout *L=g_shell_layout;
    return (L&&g_camera_shell)?*(void **)(g_camera_shell+L->camera_global):0;
}
static void write_pose(void *camera,const float *pos,const float *rot){
    U32 i;
    for(i=0;i<3;i++)((float *)((U8 *)camera+0x10))[i]=pos[i];
    for(i=0;i<4;i++)((float *)((U8 *)camera+0x1c))[i]=rot[i];
}
static void blend(float a){
    float dot=0.0f,sign=1.0f,len=0.0f;U32 i;
    for(i=0;i<3;i++)shown_pos[i]=from_pos[i]+(to_pos[i]-from_pos[i])*a;
    for(i=0;i<4;i++)dot+=from_rot[i]*to_rot[i];
    if(dot<0.0f)sign=-1.0f;
    for(i=0;i<4;i++){shown_rot[i]=from_rot[i]*(1.0f-a)+sign*to_rot[i]*a;len+=shown_rot[i]*shown_rot[i];}
    len=square_root(len);
    if(len>0.000001f)for(i=0;i<4;i++)shown_rot[i]/=len;
    else for(i=0;i<4;i++)shown_rot[i]=to_rot[i];
}
/* Replaces the call at 0xab104: ECX = camera, (position, rotation). */
void __attribute__((thiscall)) kpc_camera_set(void *camera,const float *pos,const float *rot){
    const ShellLayout *L=g_shell_layout;
    U32 now=now_ms(),i,gap=now-sample_ms;float dist=0.0f;
    samples++;
    if(!g_camera_smooth||!camera){((CameraSetFn)(g_camera_shell+L->camera_set))(camera,pos,rot);return;}
    for(i=0;i<3;i++){float d=pos[i]-shown_pos[i];dist+=d*d;}
    if(!active||gap>CAMERA_STALE_MS||dist>CAMERA_CUT_UNITS*CAMERA_CUT_UNITS){
        /* First sample, after a pause, or a cut: show it as it is. */
        ((CameraSetFn)(g_camera_shell+L->camera_set))(camera,pos,rot);
        for(i=0;i<3;i++)shown_pos[i]=from_pos[i]=to_pos[i]=pos[i];
        for(i=0;i<4;i++)shown_rot[i]=from_rot[i]=to_rot[i]=rot[i];
        if(active)snaps++;
        active=1;sample_ms=now;
        return;
    }
    if(gap<CAMERA_MIN_INTERVAL_MS)gap=CAMERA_MIN_INTERVAL_MS;
    if(gap>CAMERA_MAX_INTERVAL_MS)gap=CAMERA_MAX_INTERVAL_MS;
    interval_ms=gap;
    for(i=0;i<3;i++){from_pos[i]=shown_pos[i];to_pos[i]=pos[i];}
    for(i=0;i<4;i++){from_rot[i]=shown_rot[i];to_rot[i]=rot[i];}
    sample_ms=now;
    write_pose(camera,shown_pos,shown_rot);
}
/* Every client frame. */
void kpc_camera_tick(void){
    U32 now,elapsed;void *camera;float a;
    if(!active)return;
    now=now_ms();elapsed=now-sample_ms;
    if(elapsed>CAMERA_STALE_MS){active=0;return;}
    camera=camera_object();
    if(!camera){active=0;return;}
    a=(float)elapsed/(float)interval_ms;
    if(a>1.0f)a=1.0f;
    blend(a);
    write_pose(camera,shown_pos,shown_rot);
}
U32 kpc_camera_samples(void){return samples;}
U32 kpc_camera_snaps(void){return snaps;}
