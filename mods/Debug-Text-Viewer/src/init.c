#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0400
#include <windows.h>
#include <stdio.h>

// Functions
typedef FILE * (__cdecl *pFreopen)( 
	const char *path,
	const char *mode,
	FILE *stream 
);

typedef FILE * (__cdecl *p__iob_func)(void); // Undocumented, returns array of std files

typedef BOOL (__stdcall *pAllocConsole)(void);
typedef BOOL (__stdcall *pFreeLibrary)(HMODULE hModule);


void DebugText_Init(){
    //Load DLLs and functions from them, as they're not in Sonic R's import table
    HINSTANCE MSVCRT_DLL = LoadLibraryA("msvcrt.dll");
    HINSTANCE KERNEL32_DLL = LoadLibraryA("kernel32.dll");
    pFreopen freopen = GetProcAddress(MSVCRT_DLL, "freopen");
    pAllocConsole AllocConsole = GetProcAddress(KERNEL32_DLL, "AllocConsole");
    pFreeLibrary FreeLibrary = GetProcAddress(KERNEL32_DLL, "FreeLibrary");
    p__iob_func __iob_func = GetProcAddress(MSVCRT_DLL, "__iob_func");
    
    AllocConsole();
    freopen("CONOUT$", "w", __iob_func() + 1); // equal to stdout
    
    FreeLibrary(MSVCRT_DLL);
    FreeLibrary(KERNEL32_DLL);
}
