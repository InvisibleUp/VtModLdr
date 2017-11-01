#include <stdarg.h>
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0400
#include <windows.h>

typedef BOOL (__stdcall *pFreeLibrary)(HMODULE hModule);
typedef int (__cdecl *pVprintf)(const char *format, va_list arg);

void DebugText_Write(const char *lpFmt, ...)
{
	// Load DLLs and functions from them, as they're not in Sonic R's import table
	HINSTANCE MSVCRT_DLL = LoadLibraryA("msvcrt.dll");
	HINSTANCE KERNEL32_DLL = LoadLibraryA("kernel32.dll");
	pVprintf vprintf = GetProcAddress(MSVCRT_DLL, "vprintf");
	pFreeLibrary FreeLibrary = GetProcAddress(KERNEL32_DLL, "FreeLibrary");

	// Because we're impersonating printf(), we need to call vprintf() with our arguments
	// To do this, we need to use the va_* macros to get the value of "..." to pipe into vprintf
	va_list va;
	va_start (va, lpFmt);
	vprintf(lpFmt, va);
	va_end (va);

	// Amazingly, we don't have FreeLibrary in the import table
	// We need to load FreeLibrary and use it to free both the C runtime and itself.
	// Sketchy, but no reason it shouldn't work
	FreeLibrary(MSVCRT_DLL);
	FreeLibrary(KERNEL32_DLL);
}
