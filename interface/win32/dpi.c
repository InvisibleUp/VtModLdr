#include <windows.h>
#include "../../includes.h"
#include "win32helper.h"

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  GetSysDefaultFont
 *  Description:  Saves the default dialog font and it's size into a global variable.
 *                This is vital for the dialog scaler functions.
 * =====================================================================================
 */
void GetSysDefaultFont()
{
	NONCLIENTMETRICS MetricsConfig = {0};
	TEXTMETRIC TextMetrics = {0};
	HDC hdc = GetDC(NULL);
	
	MetricsConfig.cbSize = sizeof(NONCLIENTMETRICS);
	SystemParametersInfo(
		SPI_GETNONCLIENTMETRICS,
		MetricsConfig.cbSize,
		&MetricsConfig, 0
	);
	
	DLGFONT = CreateFontIndirect(&(MetricsConfig.lfMessageFont));
	DLGFONTSIZE = (unsigned short)MetricsConfig.lfMessageFont.lfHeight;

	//Get font size (it's convoluted)
	SelectObject(hdc, DLGFONT);
	GetTextMetrics(hdc, &TextMetrics);
	DLGFONTHEIGHT = TextMetrics.tmHeight;
	
	ReleaseDC(NULL, hdc);
	return;
}

/* 
 * ===  CALLBACK  ======================================================================
 *         Name:  SetFont
 *  Description:  Sets font for all children of a window
 *                Taken from http://stackoverflow.com/a/17075471
 *       Caller:  EnumChildWindows()
 * =====================================================================================
 */
BOOL CALLBACK SetFont(HWND child, LPARAM font)
{
	SendMessage(child, WM_SETFONT, font, TRUE);
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FixDlgFont
 *  Description:  Very hacky, strange function that preforms surgery on a dialog
 *                resource (NOT a DialogEx resource!) to change the font size to
 *                that given. (This will typically be the system's font size)
 *
 *                This is here so that dialogs scale with the display settings even
 *                on older operating systems (pre-Vista, really) without specific
 *                support for DPI scaling.
 *
 *                Look up https://blogs.msdn.microsoft.com/oldnewthing/20040621-00/?p=38793/
 *                for details on what exactly I'm doing.
 *                (Also, I know theoldnewthing isn't official documentation, but
 *                it's the best I've got. Sorry Raymond Chen.)
 *                If the guys at MS had defined DLGTEMPLATEX somewhere, we wouldn't have
 *                this problem. :/
 * =====================================================================================
 */
void FixDlgFont(DLGTEMPLATE *pTemplate, unsigned short size)
{
	unsigned short *fontSize = pTemplate; //Readdress template
	//(Yes, I know there's a warning here. No, I don't know how to fix it.)
	
	//Skip past header (gotta divide by 2 because we're going by shorts)
	fontSize += sizeof(DLGTEMPLATE)/2;
	
	//After the header comes the menu.
	//Menu can be either a numeral, a string, or a null byte.
	//Test first byte
	
	if(*fontSize == 0x0000){
		//Nothing!
		fontSize++;
	} else if (*fontSize == 0xFFFF || *fontSize == 0xFF00){
		//Technically it's supposed to be 0xFF00, but Borland's
		//resource compiler uses 0xFFFF. Not gonna argue.
		
		//This means it's a 16-bit short, so we gotta go ahead 2 shorts.
		fontSize += 2;
	} else {
		//String. Skip past it.
		fontSize += wcslen(fontSize);
		//Plus null byte
		fontSize++;
	}
	
	//Then dialog class. Same thing
	if(*fontSize == 0x0000){
		fontSize++;
	} else if (*fontSize == 0xFFFF || *fontSize == 0xFF00){
		fontSize += 2;
	} else {
		fontSize += wcslen(fontSize);
		fontSize++;
	}
	
	//Then title. Same thing.
	if(*fontSize == 0x0000){
		fontSize++;
	} else if (*fontSize == 0xFFFF || *fontSize == 0xFF00){
		fontSize += 2;
	} else {
		fontSize += wcslen(fontSize);
		fontSize++;
	}
	
	//Then set font size!
	*fontSize = size;
	return;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  CreateDialogSysFont
 *  Description:  Like CreateDialog(), but patches the font size to be the same as 
 *                the system font. That way you HiDPI folks won't go blind staring at
 *                8pt Tahoma on your 4K monitor. I hope. (This is not modal.)
 * =====================================================================================
 */
HWND CreateDialogSysFont(int DlgIDD, DLGPROC DlgProc, HWND hParent)
{
	HWND hDialog = NULL;
	HRSRC res;
	void *pTemplate = NULL;
	void *mainTemplate = NULL;
	int sizeTemplate = 0;
	OSVERSIONINFO osVer;

	osVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osVer);

	if (osVer.dwMajorVersion >= 6) {
		//Vista+ handles DPI automatically (I think?)
		return CreateDialog(CURRINSTANCE, MAKEINTRESOURCE(DlgIDD), hParent, DlgProc);
	}
	
	//Get dialog from resource file
	res = FindResource(NULL, MAKEINTRESOURCE(DlgIDD), RT_DIALOG);
	if (!res) {
		return NULL;
	}
	pTemplate = LoadResource(NULL, res);
	if (!pTemplate) {
		return NULL;
	}
	pTemplate = LockResource(pTemplate);
	
	//Duplicate template
	sizeTemplate = SizeofResource(NULL, res);
	mainTemplate = calloc(sizeTemplate, 1);
	if (!mainTemplate) {
		return NULL;
	}
	memcpy(mainTemplate, pTemplate, sizeTemplate);
	
	//Fix template
	FixDlgFont(mainTemplate, DLGFONTSIZE);
	
	//Make dialog
	hDialog = CreateDialogIndirect(CURRINSTANCE, mainTemplate, hParent, DlgProc);
	
	//safe_free(strIDD);
	safe_free(mainTemplate);
	return hDialog;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  DialogBoxSysFont
 *  Description:  Like DialogBox(), but patches the font size to be the same as 
 *                the system font. (This is modal.)
 * =====================================================================================
 */
long DialogBoxSysFont(int DlgIDD, DLGPROC DlgProc, HWND hParent)
{
	HRSRC res;
	void *pTemplate = NULL;
	void *mainTemplate = NULL;
	int sizeTemplate = 0;
	int result = 0;

	OSVERSIONINFO osVer;

	osVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osVer);

	if (osVer.dwMajorVersion >= 6) {
		//Vista+ handles DPI automatically
		return DialogBox(CURRINSTANCE, MAKEINTRESOURCE(DlgIDD), hParent, DlgProc);
	}
	
	//Get dialog from resource file
	res = FindResource(NULL, MAKEINTRESOURCE(DlgIDD), RT_DIALOG);
	if (!res) {
		DWORD temp = GetLastError();
		return -1;
	}
	pTemplate = LoadResource(NULL, res);
	if (!pTemplate) {
		return -1;
	}
	pTemplate = LockResource(pTemplate);
	
	//Duplicate template
	sizeTemplate = SizeofResource(NULL, res);
	mainTemplate = calloc(sizeTemplate, 1);
	if (!mainTemplate) {
		return -1;
	}
	memcpy(mainTemplate, pTemplate, sizeTemplate);
	
	//Fix template
	FixDlgFont(mainTemplate, DLGFONTSIZE);
	
	//Make dialog
	result = DialogBoxIndirect(CURRINSTANCE, mainTemplate, hParent, DlgProc);
	
	safe_free(mainTemplate);
	return result;
}