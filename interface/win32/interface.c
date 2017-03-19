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
int Interface_Init(int argc, char *argv[])
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

	return 0;
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
int Interface_EditConfig(void)
{
	int result = DialogBoxSysFont(IDD_PROGCONFIG, Dlg_Profile, NULL);
	if (result == IDCANCEL) { CURRERROR = errUSR_ABORT; }
	
	if (CURRERROR = errNOERR) {
		return 0;
	}
	else {
		return 1;
	}
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Interface_RunProgram
*  Description:  BLOCKING function that runs the program specified in the user's
*                profile.
* =====================================================================================
*/
int Interface_RunProgram()
{
	PROCESS_INFORMATION ProgInfo;
	STARTUPINFO StartInfo;
	MSG msg;
	DWORD reason = WAIT_TIMEOUT;

	//Set startup info struct
	StartInfo.cb = sizeof(STARTUPINFO);
	StartInfo.lpReserved = NULL;
	StartInfo.cbReserved2 = 0;
	StartInfo.lpReserved2 = NULL;
	StartInfo.lpDesktop = "";
	StartInfo.lpTitle = NULL;
	StartInfo.dwFlags = 0;

	//Create process
	CreateProcess(
		NULL, // pointer to name of executable module  
		CONFIG.RUNPATH, // pointer to command line string  
		NULL, // pointer to process security attributes 
		NULL, // pointer to thread security attributes
		FALSE, // handle inheritance flag 
		CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
		// creation flags 
		NULL, // pointer to new environment block 
		CONFIG.CURRDIR, // pointer to current directory name 
		&StartInfo, // pointer to STARTUPINFO 
		&ProgInfo // pointer to PROCESS_INFORMATION 
	);

	//Wait for process to exit.
	//Code taken from http://stackoverflow.com/a/1649847
	while (WAIT_OBJECT_0 != reason) {
		reason = MsgWaitForMultipleObjects(
			1, &ProgInfo.hProcess, FALSE, INFINITE, QS_ALLINPUT
		);
		switch (reason) {
		case WAIT_OBJECT_0:
			// Your child process is finished.
			break;
		case (WAIT_OBJECT_0 + 1):
			// A message is available in the message queue.
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			break;
		}
	}

	CloseHandle(ProgInfo.hProcess);
	CloseHandle(ProgInfo.hThread);
	return 0;
}