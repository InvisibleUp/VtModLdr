/* Private functions */
#include "../../includes.h"
#include <windows.h>               // Win32 APIs for the frontend
#include <commctrl.h>              // Common Controls for listbox, etc.
#include <Shlobj.h>                // "Open Folder" dialog
#include "resource.h"
#include "win32global.h"

//Defines
#define WMX_ERROR WM_USER+1 //For main dialog
#define WMX_RELOADPROF WM_USER+2 //For profile dialog
#define WMX_BLANKPROF WM_USER+3 //For profile dialog

//Window management
void EnableWholeWindow(HWND hwnd, BOOL state);
BOOL CALLBACK ScrollByMove(HWND hCtl, LPARAM ScrollDelta);
char *GetWindowTextMalloc(HWND hwnd);

//Preparation
BOOL Mod_InstallPrep(HWND hwnd);
BOOL Mod_UninstallPrep(HWND hwnd);

//Main dialog
BOOL UpdateModList(HWND list);
BOOL UpdateModDetails(HWND hwnd, int listID);
char *Mod_GetUUIDFromWin(HWND hwnd);;

//Variable dialog
BOOL CALLBACK Dlg_Var_Resize(HWND hCtl, LPARAM width);
BOOL CALLBACK Dlg_Var_Save(HWND hCtl, LPARAM lParam);
HWND Var_GenWindow(const char *ModUUID, HWND hDlgMain);

//Font/DPI functions
BOOL CALLBACK SetFont(HWND child, LPARAM font);
void FixDlgFont(DLGTEMPLATE *pTemplate, unsigned short size);
HWND CreateDialogSysFont(int DlgIDD, DLGPROC DlgProc, HWND hParent);
long DialogBoxSysFont(int DlgIDD, DLGPROC DlgProc, HWND hParent);
void GetSysDefaultFont(VOID);

//Dialog callbacks
LRESULT CALLBACK Dlg_Var(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK Dlg_Patch(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK Dlg_Main(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK Dlg_Generic(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK Dlg_Profile(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);