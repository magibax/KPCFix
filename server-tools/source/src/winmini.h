#ifndef KPC_WINMINI_H
#define KPC_WINMINI_H

/* Freestanding declarations for the documented x86 Windows ABI. */
typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char U8;
typedef int I32;
typedef U32 DWORD;
typedef U32 UINT;
typedef U32 SIZE_T;
typedef int BOOL;
typedef unsigned short WCHAR;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *HINSTANCE;
typedef void *HWND;
typedef I32 HRESULT;
typedef struct { U32 low; I32 high; } LARGE_INTEGER;
#define WINAPI __attribute__((stdcall))
#define CDECL __attribute__((cdecl))
#define NULL ((void *)0)
#define INVALID_HANDLE_VALUE ((HANDLE)0xffffffffu)
#define E_FAIL ((HRESULT)0x80004005u)
#define E_NOTIMPL ((HRESULT)0x80004001u)
#define S_FALSE ((HRESULT)1)
typedef void (WINAPI *FARPROC)(void);

extern HMODULE WINAPI GetModuleHandleW(const WCHAR *);
extern DWORD WINAPI GetModuleFileNameW(HMODULE,WCHAR *,DWORD);
extern BOOL WINAPI GetModuleHandleExW(DWORD,const WCHAR *,HMODULE *);
extern UINT WINAPI GetSystemDirectoryW(WCHAR *,UINT);
extern HMODULE WINAPI LoadLibraryW(const WCHAR *);
extern FARPROC WINAPI GetProcAddress(HMODULE,const char *);
extern BOOL WINAPI DisableThreadLibraryCalls(HMODULE);
extern HANDLE WINAPI GetCurrentProcess(void);
extern DWORD WINAPI GetCurrentThreadId(void);
extern DWORD WINAPI GetTickCount(void);
extern void WINAPI Sleep(DWORD);
extern DWORD WINAPI GetLastError(void);
extern void WINAPI SetLastError(DWORD);
extern BOOL WINAPI VirtualProtect(void *,SIZE_T,DWORD,DWORD *);
extern BOOL WINAPI FlushInstructionCache(HANDLE,const void *,SIZE_T);
extern HANDLE WINAPI CreateFileW(const WCHAR *,DWORD,DWORD,void *,DWORD,DWORD,HANDLE);
extern BOOL WINAPI GetFileSizeEx(HANDLE,LARGE_INTEGER *);
extern BOOL WINAPI ReadFile(HANDLE,void *,DWORD,DWORD *,void *);
extern BOOL WINAPI WriteFile(HANDLE,const void *,DWORD,DWORD *,void *);
extern BOOL WINAPI CloseHandle(HANDLE);
extern UINT WINAPI GetPrivateProfileIntW(const WCHAR *,const WCHAR *,I32,const WCHAR *);
extern int WINAPI WideCharToMultiByte(UINT,DWORD,const WCHAR *,int,char *,int,const char *,BOOL *);
extern int WINAPI MessageBoxA(HWND,const char *,const char *,UINT);

#endif
