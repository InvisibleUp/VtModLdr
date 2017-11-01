#include <windows.h>
//#include <tchar.h>
#include "resource.h"
#pragma resource "resources.res"

const char g_szClassName[] = "myWindowClass";
//_TCHAR szBuffer2[100]; //for debug messageboxes

// About Dialog Procedure
BOOL CALLBACK AboutDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	struct SL_Line *linelist;
	int linelistlen = 0;
	
	switch(Message){
	case WM_INITDIALOG:
		return TRUE;
	
	case WM_PAINT:
		return TRUE;
	
	case WM_COMMAND:
		switch(LOWORD(wParam)){
			case IDOK:
				EndDialog(hwnd, IDOK);
			break;
			case IDCANCEL:
				EndDialog(hwnd, IDCANCEL);
			break;
		}
	break;
	default:
		return FALSE;
	}
	
	return TRUE;
}

// Main Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg){
	case WM_LBUTTONDOWN:{
	    char szFileName[MAX_PATH];
		GetModuleFileName(NULL, szFileName, MAX_PATH);
		MessageBox(hwnd, szFileName, "This program is:", MB_OK | MB_ICONINFORMATION);
	}
	break;
	
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case ID_FILE_EXIT:{
			PostMessage(hwnd, WM_CLOSE, 0, 0);
		}
		break;
		case ID_STUFF_GO:{
			DialogBox(GetModuleHandle(NULL),
				MAKEINTRESOURCE(IDD_RESIZE), hwnd, AboutDlgProc);
		}
		break;
		case ID_HELP_ABOUT:{
			/*int ret = */DialogBox(GetModuleHandle(NULL),
				MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);
		}
		break;
	} 
	break;
		
	case WM_CLOSE:
		MessageBox(NULL, "Goodbye, cruel world!", "Note", MB_OK);
		DestroyWindow(hwnd);
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
	break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;

	//Step 1: Registering the Window Class
	wc.cbSize		= sizeof(WNDCLASSEX);
	wc.style		 = 0;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= 0;
	wc.hInstance	 = hInstance;
	wc.hIcon		 = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor	   = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND+1);
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MYMENU);
	wc.lpszClassName = g_szClassName;
	wc.hIconSm	   = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MYICON), IMAGE_ICON, 16, 16, 0);

	if(!RegisterClassEx(&wc)){
		MessageBox(NULL, "Window Registration Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// Step 2: Creating the Window
	hwnd = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);

	if(hwnd == NULL){
		MessageBox(NULL, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	// Step 3: The Message Loop
	while(GetMessage(&Msg, NULL, 0, 0) > 0){
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return Msg.wParam;
}
