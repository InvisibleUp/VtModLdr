#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0400
#include <windows.h>

typedef void (__stdcall *pInitFunct)(void);
extern int InitCount;
extern pInitFunct *HookAddr;
extern int HookCount;

void InitLoop(void){
    for(int i = 0; i < (int)&HookCount; i++){
        pInitFunct CurrAddr = &(HookAddr[i]);
        CurrAddr();
    }
}
