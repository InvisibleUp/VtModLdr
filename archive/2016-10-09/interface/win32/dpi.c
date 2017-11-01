#include <windows.h>
#include "../../includes.h"
#include "win32helper.h"

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

//Callback to set font for all children of a window
//Use with EnumChildWindows
//Taken from http://stackoverflow.com/a/17075471
BOOL CALLBACK SetFont(HWND child, LPARAM font)
{
	SendMessage(child, WM_SETFONT, font, TRUE);
	return TRUE;
}

//Fix a dialog resource so that it uses the given font size
void FixDlgFont(DLGTEMPLATE *pTemplate, unsigned short size)
{
	unsigned short *fontSize = pTemplate; //Readdress template
	//(Yes, I know there's a warning here. No, I don't know how to fix it.)
	
	//Look up https://blogs.msdn.microsoft.com/oldnewthing/20040621-00/?p=38793/
	//for details on what exactly I'm doing. It's weird and hacky.
	//(Also, I know theoldnewthing isn't official documentation, but
	//it's the best I've got. Sorry Raymond Chen.)
	//If the guys as MS had defined DLGTEMPLATEX somewhere, we wouldn't have
	//this problem. :/
	
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

//Like CreateDialog(), but patches the font size to be the same as the system
//font. That way you HiDPI folks won't go blind staring at 8pt Tahoma on
//your 4K monitor. I hope.
HWND CreateDialogSysFont(int DlgIDD, DLGPROC DlgProc, HWND hParent)
{
	HWND hDialog = NULL;
	char *strIDD = NULL;
	HRSRC res;
	void *pTemplate = NULL;
	void *mainTemplate = NULL;
	int sizeTemplate = 0;
	
	//Create string for IDD_MAIN
	asprintf(&strIDD, "#%u", DlgIDD); 
	
	//Get dialog from resource file
	res = FindResource(NULL, strIDD, RT_DIALOG);
	pTemplate = LoadResource(NULL, res);
	pTemplate = LockResource(pTemplate);
	
	//Duplicate template
	sizeTemplate = SizeofResource(NULL, res);
	mainTemplate = calloc(sizeTemplate, 1);
	memcpy(mainTemplate, pTemplate, sizeTemplate);
	
	//Fix template
	FixDlgFont(mainTemplate, DLGFONTSIZE);
	
	//Make dialog
	hDialog = CreateDialogIndirect(CURRINSTANCE, mainTemplate, hParent, DlgProc);
	
	safe_free(strIDD);
	safe_free(mainTemplate);
	return hDialog;
}

//Literally the same as above, but modal
long DialogBoxSysFont(int DlgIDD, DLGPROC DlgProc, HWND hParent)
{
	char *strIDD = NULL;
	HRSRC res;
	void *pTemplate = NULL;
	void *mainTemplate = NULL;
	int sizeTemplate = 0;
	int result = 0;
	
	//Create string for IDD_MAIN
	asprintf(&strIDD, "#%u", DlgIDD); 
	
	//Get dialog from resource file
	res = FindResource(NULL, strIDD, RT_DIALOG);
	pTemplate = LoadResource(NULL, res);
	pTemplate = LockResource(pTemplate);
	safe_free(strIDD);
	
	//Duplicate template
	sizeTemplate = SizeofResource(NULL, res);
	mainTemplate = calloc(sizeTemplate, 1);
	memcpy(mainTemplate, pTemplate, sizeTemplate);
	
	//Fix template
	FixDlgFont(mainTemplate, DLGFONTSIZE);
	
	//Make dialog
	result = DialogBoxIndirect(CURRINSTANCE, mainTemplate, hParent, DlgProc);
	

	safe_free(mainTemplate);
	return result;
}