#include <windows.h>
#include "../../includes.h"
#include "win32helper.h"
#include "interface.h"
#include "resource.h"

// This file is designed to bridge any given interface with the rest of the code.
// This keeps the "buisness logic" seperate from the GUI in the event I want to
// port this to another system (or to, say, QT.)

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Interface_Init
 *  Description:  Registers window classes, gets default font, etc.
 *                Essentially initializes the interface.
 * =====================================================================================
 */
void Interface_Init(void)
{
	//int DPIx, DPIy;
	WNDCLASS wc = {0};
	
	//Define default font
	GetSysDefaultFont();
	
	//Get system DPI
	/*{
		HDC hdc = GetDC(NULL);
		DPIx = GetDeviceCaps(hdc, LOGPIXELSX);
		DPIy = GetDeviceCaps(hdc, LOGPIXELSY);
	}*/
	//...do something with it?
	
	//Define VarWindow class
	wc.lpszClassName = "VarWindow";
	wc.style = 0;
	wc.lpfnWndProc = Dlg_Var;
	wc.hInstance = CURRINSTANCE;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszMenuName = NULL;
	RegisterClass(&wc);

}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Interface_MainLoop
 *  Description:  Main command loop that handles user input and preforms actions
 *                accordingly. On Win32 this is delegated to the Dlg_Main dialog.
 * =====================================================================================
 */
int Interface_MainLoop(void)
{
	int status;
	MSG msg;
	HWND hDialog = CreateDialogSysFont(IDD_MAIN, Dlg_Main, NULL);
	
	if (!hDialog){
		//char buf[255];
		//sprintf (buf, "Error 0x%x.", (unsigned int)GetLastError());
		char *buf;
		asprintf(&buf, "Error 0x%x", (unsigned int)GetLastError());
		AlertMsg (buf, "CreateDialog");
		safe_free(buf);
		return 1;
	}

	while (
		(status = GetMessage(&msg, 0, 0, 0)) != 0 &&
		CURRERROR != errUSR_QUIT
	){
		if (status == -1){return -1;}
		if (!IsDialogMessage(hDialog, &msg)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Interface_EditConfig
 *  Description:  Opens a dialog or equivelent that allows the user to edit the
 *                configuration. This is called on program start tp verify a pre-loaded
 *                profile or to generate one if it is missing.
 * =====================================================================================
 */
void Interface_EditConfig(void)
{
	int result = DialogBoxSysFont(IDD_PROGCONFIG, Dlg_Profile, NULL);
	if(result == IDCANCEL){CURRERROR = errUSR_ABORT;}
	return;
}