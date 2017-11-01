#include <windows.h>
#include "../../includes.h"
#include "win32helper.h"

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  AlertMsg
 *  Description:  Wrapper for Win32 MessageBox.
 *                The point here is cross-platform/UI compatibility.
 * =====================================================================================
 */
void AlertMsg (const char *Message, const char *Title)
{
	MessageBox(0, Message, Title, MB_ICONEXCLAMATION|MB_OK);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ProgDialog_Init
 *  Description:  Returns a dialog or other visible indicator with a progress
 *                bar from 0 to a given maximum value and a label.
 * =====================================================================================
 */
HWND ProgDialog_Init(int max, const char *label)
{
	HWND ProgDialog;
	//Create progress window in modeless mode
	ProgDialog = CreateDialogSysFont(IDD_PROGRESS, Dlg_Generic, NULL);
	if(ProgDialog == NULL){
		CURRERROR = errCRIT_FUNCT;
		return NULL;
	}
	
	//Set progress bar maximum
	SendMessage(
		GetDlgItem(ProgDialog, IDC_PROGBAR),
		PBM_SETRANGE, 0,
		MAKELPARAM(0, max)
	);

	//Set text label
	SetWindowText(ProgDialog, label);
	return ProgDialog;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ProgDialog_Update
 *  Description:  Modifies the progress indicator in the dialog/etc. returned
 *                by ProgDialog_Init by the amount Delta
 * =====================================================================================
 */
void ProgDialog_Update(HWND ProgDialog, int Delta)
{
	HWND ProgBar;
	MSG message;
	
	// Get progress bar control.
	ProgBar = GetDlgItem(ProgDialog, IDC_PROGBAR);
	
	// Update bar
	SendMessage(ProgBar, PBM_DELTAPOS, Delta, 0);
	//Weird Aero hack so you can actually see progress. Curse you Aero.
	SendMessage(ProgBar, PBM_DELTAPOS, -1, 0);
	SendMessage(ProgBar, PBM_DELTAPOS, 1, 0);

	// Yield control to Windows (to pretend to be responsive)
	if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
	
	return;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ProgDialog_Kill
 *  Description:  Destroys the dialog/etc. returned by ProgDialog_Init
 * =====================================================================================
 */
void ProgDialog_Kill(HWND ProgDialog)
{
	SendMessage(ProgDialog, WM_CLOSE, 0, 0);
}