#include <windows.h>
#include "../../includes.h"
#include "win32helper.h"
#include "interface.h"
#include "resource.h"

void InitInterface(void)
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

int ViewModList(void)
{
	int status;
	MSG msg;
	HWND hDialog = CreateDialogSysFont(IDD_MAIN, Dlg_Main, NULL);
	
	if (!hDialog){
		char buf[255];
		sprintf (buf, "Error 0x%x.", (unsigned int)GetLastError());
		AlertMsg (buf, "CreateDialog");
		return 1;
	}

	while (
		(status = GetMessage(&msg, 0, 0, 0)) != 0 &&
		CURRERROR != errUSR_QUIT
	){
		if (status == -1){return -1;}
		if (!IsDialogMessage(hDialog, & msg)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}

void EditConfig(void)
{
	int result = DialogBoxSysFont(IDD_PROGCONFIG, Dlg_Profile, NULL);
	if(result == IDCANCEL){CURRERROR = errUSR_ABORT;}
	return;
}