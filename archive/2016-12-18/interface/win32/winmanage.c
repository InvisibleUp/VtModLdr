#include <windows.h>
#include "../../includes.h"
#include "win32helper.h"

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  EnableWholeWindow
 *  Description:  Enables or disables all elements in a window and repaints it.
 *                Useful for preventing user actions during installation or work
 * =====================================================================================
 */
void EnableWholeWindow(HWND hwnd, BOOL state)
{
	HWND hCtl = GetWindow(hwnd,GW_CHILD);
	HMENU hMenu = GetMenu(hwnd);
	int MenuCount = GetMenuItemCount(hMenu);
	int i;

	//Disable all controls, but not main window.
	//This prevents window from flying into BG.
	while (hCtl) {
		EnableWindow(hCtl, state);
		hCtl = GetWindow(hCtl, GW_HWNDNEXT);
	}

	//Plus also the menus.
	if(MenuCount < 0){MenuCount = 0;} //Prevent deadlock/crash if no menu
	for(i = 0; i < MenuCount; i++){
		EnableMenuItem(hMenu, i, MF_BYPOSITION | (state ? MF_ENABLED : MF_GRAYED));
	}


	RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE || RDW_UPDATENOW || RDW_ALLCHILDREN);
	DrawMenuBar(hwnd);
}

//Scroll a window by moving it's contents!
//Because, unsurprisingly, ScrollWindowEx() is worthless.
BOOL CALLBACK ScrollByMove(HWND hCtl, LPARAM ScrollDelta)
{
	RECT rectCtl;
	GetWindowRect(hCtl, &rectCtl);
	MapWindowPoints(HWND_DESKTOP, GetParent(hCtl), (LPPOINT) &rectCtl, 2);
	
	MoveWindow(
		hCtl,
		rectCtl.left,                  	//X
		rectCtl.top + ScrollDelta,     	//Y
		(rectCtl.right - rectCtl.left),	//Width
		(rectCtl.bottom - rectCtl.top),	//Height
		TRUE
	);
	
	return TRUE;
}

//Wraps GetWindowText() so that result is malloc'd.
char *GetWindowTextMalloc(HWND hwnd)
{
	int len = GetWindowTextLength(hwnd);
	char *outBuffer = malloc(len+1);
	if (!outBuffer) {
		CURRERROR = errCRIT_MALLOC;
		return NULL;
	}
	GetWindowText(hwnd, outBuffer, len+1);
	return outBuffer;
}
