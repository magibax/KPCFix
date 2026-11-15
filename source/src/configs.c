/* Narrow, startup-only config migration. Unknown settings/line endings survive.
 * Originals are backed up once; replacement is flushed then renamed atomically.
 * Game saves stay at zero through the console/profile runtime hooks. */
#include "ui.h"
#define CAP 131072u
#define PATH_CAP 260u
static U8 input[CAP],output[CAP+64];
typedef struct {U32 attributes,times[6],size_high,size_low,reserved[2];WCHAR name[260],alternate[14];} FindData;
extern BOOL WINAPI CopyFileW(const WCHAR *,const WCHAR *,BOOL);
extern BOOL WINAPI MoveFileExW(const WCHAR *,const WCHAR *,DWORD);
extern BOOL WINAPI DeleteFileW(const WCHAR *);
extern BOOL WINAPI FlushFileBuffers(HANDLE);
extern HANDLE WINAPI FindFirstFileW(const WCHAR *,FindData *);
extern BOOL WINAPI FindNextFileW(HANDLE,FindData *);
extern BOOL WINAPI FindClose(HANDLE);

static int space(U8 c){return c==' '||c=='\t';}
static U8 lower(U8 c){return c>='A'&&c<='Z'?(U8)(c+32):c;}
static int path_append(WCHAR *out,const WCHAR *path,const WCHAR *tail){
    U32 i=0,j=0;while(path[i]){if(i+1>=PATH_CAP)return 0;out[i]=path[i];i++;}
    while(tail[j]){if(i+1>=PATH_CAP)return 0;out[i++]=tail[j++];}out[i]=0;return 1;
}
static int emit(U32 *n,const U8 *s,U32 length){
    U32 i;if(*n+length>sizeof(output))return 0;
    for(i=0;i<length;i++)output[(*n)++]=s[i];return 1;
}

/* Returns output size or 0xffffffff for unsupported/broken input. Idempotent.
 * Only a whole InputRate setting key at line start is recognized, never comments
 * or values containing that word. Existing duplicate settings are all fixed. */
U32 CDECL kpc_normalize_text(const U8 *text,U32 size,U8 *destination,U32 capacity){
    static const U8 canonical[]="\"InputRate\" \"0\"";
    U32 line=0,n=0,found=0,i,j,start,end,key,tail,match;
    if(size>CAP)return 0xffffffffu;
    for(i=0;i<size;i++)if(!text[i])return 0xffffffffu; /* excludes UTF-16 */
    while(line<size){
        end=line;while(end<size&&text[end]!='\r'&&text[end]!='\n')end++;
        start=line;while(start<end&&space(text[start]))start++;
        if(line==0&&end>=3&&text[0]==0xef&&text[1]==0xbb&&text[2]==0xbf){start=3;while(start<end&&space(text[start]))start++;}
        key=start;if(key<end&&text[key]=='"')key++;
        match=key+9<=end;
        for(j=0;match&&j<9;j++)if(lower(text[key+j])!=(U8)"inputrate"[j])match=0;
        tail=key+9;
        if(match){
            if(key!=start){if(tail>=end||text[tail]!='"')match=0;else tail++;}
            if(tail<end&&!space(text[tail]))match=0;
        }
        if(match){
            found=1;while(tail<end&&space(text[tail]))tail++;
            if(tail<end&&text[tail]=='"'){
                tail++;while(tail<end&&text[tail]!='"')tail++;
                if(tail==end)return 0xffffffffu;tail++;
            }else{while(tail<end&&!space(text[tail])&&text[tail]!='#'&&text[tail]!=';')tail++;}
            if(!emit(&n,text+line,start-line)||!emit(&n,canonical,sizeof(canonical)-1)||!emit(&n,text+tail,end-tail))return 0xffffffffu;
        }else if(!emit(&n,text+line,end-line))return 0xffffffffu;
        i=end;if(i<size&&text[i]=='\r')i++;if(i<size&&text[i]=='\n')i++;
        if(!emit(&n,text+end,i-end))return 0xffffffffu;line=i;
    }
    if(!found){
        if(n&&output[n-1]!='\r'&&output[n-1]!='\n'&&!emit(&n,(const U8 *)"\r\n",2))return 0xffffffffu;
        if(!emit(&n,canonical,sizeof(canonical)-1)||!emit(&n,(const U8 *)"\r\n",2))return 0xffffffffu;
    }
    if(n>capacity)return 0xffffffffu;
    for(i=0;i<n;i++)destination[i]=output[i];return n;
}

static void normalize_file(const WCHAR *path){
    WCHAR backup[PATH_CAP],temporary[PATH_CAP];HANDLE h=INVALID_HANDLE_VALUE;
    LARGE_INTEGER length;DWORD got=0,written,error;U32 n,i;int changed=0,temp_created=0;
    if(!path_append(backup,path,L".kpc-inputrate.bak")||!path_append(temporary,path,L".kpc-inputrate.tmp"))goto fail;
    h=CreateFileW(path,0x80000000u,1,NULL,3,0x80,NULL);
    if(h==INVALID_HANDLE_VALUE){
        error=GetLastError();if(error==2||error==3)return;goto fail;
    }
    if(!GetFileSizeEx(h,&length)||length.high||length.low>CAP)goto fail;
    if(!ReadFile(h,input,length.low,&got,NULL)||got!=length.low)goto fail;
    CloseHandle(h);h=INVALID_HANDLE_VALUE;
    n=kpc_normalize_text(input,got,output,sizeof(output));if(n==0xffffffffu)goto fail;
    changed=n!=got;
    for(i=0;!changed&&i<n;i++)if(input[i]!=output[i])changed=1;
    if(!changed)return;
    if(!CopyFileW(path,backup,1)){
        error=GetLastError();if(error!=80&&error!=183)goto fail;
    }
    h=CreateFileW(temporary,0x40000000u,0,NULL,1,0x80,NULL); /* CREATE_NEW */
    if(h==INVALID_HANDLE_VALUE)goto fail;temp_created=1;
    if(!WriteFile(h,output,n,&written,NULL)||written!=n||!FlushFileBuffers(h))goto fail;
    if(!CloseHandle(h)){h=INVALID_HANDLE_VALUE;goto fail;}h=INVALID_HANDLE_VALUE;
    if(!MoveFileExW(temporary,path,9))goto fail; /* REPLACE_EXISTING | WRITE_THROUGH */
    kpc_config_notice(path,"InputRate config set to zero (original backup retained): ");return;
fail:
    if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
    if(temp_created)DeleteFileW(temporary);
    kpc_config_notice(path,"InputRate config migration skipped/failed; runtime lock remains active: ");
}

void CDECL kpc_normalize_configs(const WCHAR *main_path){
    WCHAR root[PATH_CAP],path[PATH_CAP],profiles[PATH_CAP];U32 n=0,last=0,i;
    HANDLE search;FindData data;
    while(n<PATH_CAP&&main_path[n]){if(main_path[n]=='\\'||main_path[n]=='/')last=n+1;n++;}
    if(n==PATH_CAP||!last)return;
    for(i=0;i<last;i++)root[i]=main_path[i];root[last]=0;
    if(path_append(path,root,L"autoexec.cfg"))normalize_file(path);
    if(!path_append(profiles,root,L"Profiles\\")||!path_append(path,profiles,L"*.cfg"))return;
    search=FindFirstFileW(path,&data);if(search==INVALID_HANDLE_VALUE)return;
    do{
        if(!(data.attributes&(16u|0x400u))&&path_append(path,profiles,data.name))normalize_file(path);
    }while(FindNextFileW(search,&data));
    FindClose(search);
}
