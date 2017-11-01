#include <windows.h>
#include "../../includes.h"
#include "win32helper.h"
#include "interface.h"

HBITMAP prevBitmap = NULL; //Preview bitmap.

BOOL CALLBACK Dlg_Var_Resize(HWND hCtl, LPARAM width)
{
	long wCtl = width / 3; //33%!
	long xCtl = width - (7 + wCtl);
	long wLbl = width - xCtl - 10;
	char ctlClass[64];
	RECT ctlRect;
	BOOL HasBuddy = FALSE;
	
	//Get child type
	GetClassName(hCtl, ctlClass, 64);
	HasBuddy = 
		GetProp(hCtl, "HasBuddy") &&
		(strcmp(GetProp(hCtl, "HasBuddy"), "True") == 0);
	//Get sizes
	GetWindowRect(hCtl, &ctlRect);
	MapWindowPoints(HWND_DESKTOP, GetParent(hCtl), (LPPOINT) &ctlRect, 2);
	
	//Check child type
	if(
		(strcmp(ctlClass, "Button") == 0 || //Checkbox
		strcmp(ctlClass, "Edit") == 0 || //Numeric
		strcmp(ctlClass, "ComboBox") == 0) && //ListBox
		HasBuddy == FALSE
	){
		//Control!
		MoveWindow(
			hCtl,
			xCtl, ctlRect.top,
			wCtl, (ctlRect.bottom - ctlRect.top),
			TRUE
		);
	} else if (
		strcmp(ctlClass, "Edit") == 0 && //Numeric
		HasBuddy == TRUE
	){
		//Control w/ buddy
		MoveWindow(
			hCtl,
			xCtl, ctlRect.top,
			wCtl - 12, //TODO: This size is wrong w/ larger font sizes
			(ctlRect.bottom - ctlRect.top),
			TRUE
		);
	} else if (
		strcmp(ctlClass, "Static") == 0 // Label
	){
		MoveWindow(
			hCtl,
			ctlRect.left, ctlRect.top,
			wLbl, (ctlRect.bottom - ctlRect.top),
			TRUE
		);
	} else if(
		strcmp(ctlClass, "msctls_updown32") == 0 //Up-Down for Numeric
	){
		MoveWindow(
			hCtl,
			xCtl + wCtl - 14, //TODO: This size is wrong w/ larger font sizes
			ctlRect.top,
			(ctlRect.right - ctlRect.left),
			(ctlRect.bottom - ctlRect.top),
			TRUE
		);
	} else {
		//Dunno, don't care.
		//No need to mess with.
	}
	
	return TRUE;
}

BOOL CALLBACK Dlg_Var_Save(HWND hCtl, LPARAM lParam)
{
	//For each control:
	char *VarUUID = NULL;
	char *CtlType = NULL;
	struct VarValue newVar;
	
	CURRERROR = errNOERR;
	
	//Make sure it *is* a control
	VarUUID = GetProp(hCtl, "VarUUID");
	if (VarUUID == NULL){return TRUE;}
	
	//Make a buffer for the new variable and populate with old value
	newVar.UUID = GetProp(hCtl, "VarUUID");
	newVar = Var_GetValue_SQL(newVar.UUID);
	
	//Switch depending on control type
	CtlType = GetProp(hCtl, "CtlType");
	
	if(!CtlType){
		CURRERROR = errCRIT_FUNCT;
		return FALSE;
	} else if(strcmp(CtlType, "Numeric") == 0 || strcmp(CtlType, "HexNum") == 0){
		char NumText[128];
		
		//Get contents of buffer
		GetWindowText(hCtl, NumText, 128);
		
		//Force contents to numeric
		if(newVar.type == IEEE32){
			newVar.IEEE32 = (float)strtod(NumText, NULL);
		} else if (newVar.type == IEEE64){
			newVar.IEEE64 = strtod(NumText, NULL);
		} else {
			newVar.uInt32 = strtol(NumText, NULL, 0);
		}

	} else if(strcmp(CtlType, "Checkbox") == 0){
		//Get value of checkbox
		switch(SendMessage(hCtl, BM_GETCHECK, 0, 0)){
		default:
		case BST_CHECKED:
			newVar.uInt32 = 0;
		break;
		case BST_UNCHECKED:
			newVar.uInt32 = 1;
		break;
		}
	} else if (strcmp(CtlType, "List") == 0){
		long index = SendMessage(hCtl, CB_GETCURSEL, 0, 0);
		newVar.uInt32 = SendMessage(hCtl, CB_GETITEMDATA, index, 0);
	}
	
	//Restore the variable in the SQL
	Var_UpdateEntry(newVar);
	
	//Free junk we've made
	safe_free(newVar.UUID);
	safe_free(newVar.desc);
	safe_free(newVar.publicType);
	safe_free(newVar.mod);
	
	return TRUE;
}

//Copy Public Variable Listbox entires to new ListBox (Win32)
HWND Dlg_Var_CreateListBox(HWND hParent, const char *VarUUID)
{
	HWND NewListBox;
	json_t *StoredListBox;
	json_t *row;
	size_t i;
	CURRERROR = errNOERR;
	
	//Get stored contents
	{
		char *query = "SELECT * FROM VarList WHERE Var = ?;";
		sqlite3_stmt *command;
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, VarUUID, -1, SQLITE_STATIC)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return NULL;
		}
		StoredListBox = SQL_GetJSON(command);
		sqlite3_finalize(command);
	}
	
	//Create ListBox
	NewListBox = CreateWindow(
		"ComboBox", "",
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
		0, 0, 0, 0, 
		hParent, NULL,
		CURRINSTANCE, NULL
	);
	
	//Add elements to ListBox
	json_array_foreach(StoredListBox, i, row){
		char *Label = JSON_GetStr(row, "Label");
		int Value = JSON_GetInt(row, "Number");
		long ItemIndex = SendMessage(NewListBox, CB_ADDSTRING, 0, (LPARAM)Label);
		SendMessage(NewListBox, CB_SETITEMDATA, ItemIndex, Value);
		safe_free(Label);
	}	
	
	return NewListBox;
}

LRESULT CALLBACK Dlg_Var(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDCANCEL:
			SendMessage(hwnd, WM_CLOSE, 0, IDCANCEL);
			break;
		case IDOK:
			SendMessage(hwnd, WM_CLOSE, 0, IDOK);
			break;
		}
	break;
	
	case WM_VSCROLL:{
		int ScrollPos, ScrollPosNew, ScrollDelta, TrackPos;
		int ViewSize, TotalSize, LineSize;
		int *LineSizePtr;
		RECT ClientRect;
		
		if(lParam){
			//Scroll message sent by child
			break;
		}
		
		//Get position, total size, and viewsize of scrollbar
		{
			SCROLLINFO SbInfo;
			SbInfo.cbSize = sizeof(SCROLLINFO);
			SbInfo.fMask = SIF_ALL;
			GetScrollInfo(hwnd, SB_VERT, &SbInfo);
			
			ScrollPos = SbInfo.nPos;
			ViewSize = SbInfo.nPage;
			TotalSize = SbInfo.nMax;
			TrackPos = SbInfo.nTrackPos;
		}
		
		//Get line size from storage
		LineSizePtr = GetProp(hwnd, "LineCount");
		LineSize = TotalSize / *LineSizePtr;
		
		//Get window area
		GetClientRect(hwnd, &ClientRect);
		
		switch(LOWORD(wParam)){
		case SB_TOP:
			ScrollPosNew = 0;
		break;
		case SB_BOTTOM:
			ScrollPosNew = TotalSize;
		break;
		
		case SB_PAGEUP:
			ScrollPosNew = ScrollPos - ViewSize;
		break;
		case SB_PAGEDOWN:
			ScrollPosNew = ScrollPos + ViewSize;
		break;
		
		case SB_LINEUP:
			ScrollPosNew = ScrollPos - LineSize;
		break;
		case SB_LINEDOWN:
			ScrollPosNew = ScrollPos + LineSize;
		break;
		
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			ScrollPosNew = TrackPos;
		break;

		default:
			ScrollPosNew = ScrollPos;
		break;
		}
		
		ScrollPosNew = min(ScrollPosNew, TotalSize - ViewSize + 1);
		ScrollPosNew = max(0, ScrollPosNew); 
		
		{
			SCROLLINFO SbInfo;
			
			//Set scroll box position
			SbInfo.cbSize = sizeof(SCROLLINFO);
			SbInfo.fMask = SIF_POS;
			SbInfo.nPos = ScrollPosNew;
			SetScrollInfo(hwnd, SB_VERT, &SbInfo, TRUE);
			
			//Then get it, because Windows probably adjusted it
			//GetScrollInfo(hwnd, SB_VERT, &SbInfo);
		//	ScrollPosNew = SbInfo.nPos;
			
			//Then redo the ScrollDelta
			ScrollDelta = (ScrollPos - ScrollPosNew);
		}
		
		//Scroll window
		EnumChildWindows(hwnd, ScrollByMove, ScrollDelta);
		
		//Redraw window
		InvalidateRect(hwnd, &ClientRect, FALSE);

	break;
	}
	
	case WM_SIZE:{
		int Width = LOWORD(lParam);
		int Height = HIWORD(lParam);
		int *LineCountPtr = GetProp(hwnd, "LineCount");
		
		//Check window (VarWin or BigWin?)
		if(LineCountPtr){
			///VarWin (only window with lines to count)
			
			//Move controls
			EnumChildWindows(hwnd, Dlg_Var_Resize, Width);
			
			//Recalculate scrollbar size
			{
				int HeightDelta;
				SCROLLINFO VarWinScroll;
				VarWinScroll.cbSize = sizeof(SCROLLINFO);
				VarWinScroll.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
				GetScrollInfo(hwnd, SB_VERT, &VarWinScroll);
				
				HeightDelta = Height - VarWinScroll.nPage;
				
				VarWinScroll.fMask = SIF_PAGE | SIF_DISABLENOSCROLL;
				VarWinScroll.nPage = Height; //Size of viewport
				
				//Move page if resizing causes page to be out of bounds
				if(
					VarWinScroll.nPos + Height > VarWinScroll.nMax && //OOB
					VarWinScroll.nMax > Height //Scrollbar still exists
				){
					RECT ClientRect;
					//Scroll window
					EnumChildWindows(hwnd, ScrollByMove, HeightDelta);
					//Redraw window
					InvalidateRect(hwnd, &ClientRect, FALSE);
				}
				
				SetScrollInfo(hwnd, SB_VERT, &VarWinScroll, TRUE);
			}
		} else {
			///BigWin
			HWND ctlOK, ctlCancel, VarWin;
			RECT ctlOKSize, BigWinSize;
			int ctlOKHeight, ctlOKWidth;
			
			//Get height of IDOK
			ctlOK = FindWindowEx(hwnd, NULL, "BUTTON", "OK");
			GetClientRect(ctlOK, &ctlOKSize);
			ctlOKHeight = ctlOKSize.bottom - ctlOKSize.top;
			ctlOKWidth = ctlOKSize.right - ctlOKSize.left;
			
			//Resize VarWin
			VarWin = FindWindowEx(hwnd, NULL, "VarWindow", "InternalWindow");
			MoveWindow(
				VarWin, 0, 0, Width,
				Height - (ctlOKHeight + 14), FALSE
			);
			
			//Move buttons
			ctlCancel = FindWindowEx(hwnd, NULL, "BUTTON", "Cancel");
			MoveWindow(
				ctlCancel,
				Width - (ctlOKWidth + 7) - (ctlOKWidth + 4),
				Height - (ctlOKHeight + 7), 
				ctlOKWidth, ctlOKHeight,
				FALSE
			);
			MoveWindow(
				ctlOK,
				Width - (ctlOKWidth + 7),
				Height - (ctlOKHeight + 7), 
				ctlOKWidth, ctlOKHeight,
				FALSE
			);
			
			//Redraw window (causes flicker, but it's the only way)
			GetClientRect(hwnd, &BigWinSize);
			InvalidateRect(hwnd, &BigWinSize, TRUE);
		}
	break;
	}
	
	case WM_CLOSE:
		switch(lParam){
		case IDOK:{
			AlertMsg("Saving changes!", "");
			EnumChildWindows(hwnd, Dlg_Var_Save, 0);
			//Check for failure
			if(CURRERROR != errNOERR){
				SendMessage(hwnd, WMX_ERROR, 0, 0);
			}
			DestroyWindow(hwnd);
		break;
		}
	
		default:
			DestroyWindow(hwnd);
		break;
		
		}
	break;
	
	case WMX_ERROR:{
		ErrCracker(CURRERROR); //Just in case
	return TRUE;
	}
	
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	
	return 0;
}

//Generate a window containing a mod's variables
HWND Var_GenWindow(const char *ModUUID, HWND hDlgMain)
{
	//Define variables
	HWND BigWin = NULL;
	HWND VarWin = NULL;

	json_t *VarArray, *VarCurr_JSON;
	size_t i;
	int lineCount = 0;
	RECT BigWinSize, VarWinSize;//, ConfWinSize;
	
	//Set window title
	char *winTitle = NULL;
	asprintf(&winTitle, "Options for %s", ModUUID);
	
	//Recalculate Width/Height to be in dialog units
	BigWinSize.left = 0;
	BigWinSize.top = 0;
	BigWinSize.right = 245;
	BigWinSize.bottom = 160;
	MapDialogRect(hDlgMain, &BigWinSize);

	//Create window to place sub-windows in
	BigWin = CreateWindow(
		"VarWindow",	//VarWindow class we just made
		winTitle,	//Set window title later
		WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,	//Default position
		CW_USEDEFAULT,
		BigWinSize.right,	//Some width
		BigWinSize.bottom,	//Some height
		NULL,	//Child of main dialog
		NULL,	//No menu
		CURRINSTANCE,	//Window instance
		NULL	//No parameters
	);
	if(BigWin == NULL){
		AlertMsg("CreateWindow 1 failed", "");
		return NULL;
	}
	
	//Create windows to put variables in
	VarWinSize.left = 0;
	VarWinSize.top = 0;
	VarWinSize.right = 240;
	VarWinSize.bottom = 120;
	MapDialogRect(hDlgMain, &VarWinSize);
	
	VarWin = CreateWindow(
		"VarWindow", "InternalWindow",
		WS_VISIBLE | WS_CHILD | WS_VSCROLL,
		VarWinSize.left, VarWinSize.top, 
		VarWinSize.right, VarWinSize.bottom,
		BigWin,	//Child of main dialog
		NULL,	//No menu
		CURRINSTANCE,	//Window instance
		NULL	//No parameters
	);
	if(VarWin == NULL){
		AlertMsg("CreateWindow 2 failed", "");
		return NULL;
	}

	///Populate window
	//Get public variables
	{
		sqlite3_stmt *command;
		const char *query = "SELECT * FROM Variables WHERE "
		                    "PublicType IS NOT NULL AND Mod = ?;";
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			//Destroy window
			return NULL;
		}
		
		VarArray = SQL_GetJSON(command);
		
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			//Destroy window
			return NULL;
		}
	}
	
	//Make sure json output is valid
	if(!json_is_array(VarArray)){
		CURRERROR = errCRIT_DBASE;
		//Destroy window
		return NULL;
	}
	
	//Loop through rows
	json_array_foreach(VarArray, i, VarCurr_JSON){
		//Define location information for controls and labels
		//These numbers don't even matter...
		const int wCtl = VarWinSize.right / 3;
		const int hCtl = DLGFONTHEIGHT + 7;
		const int xCtl = VarWinSize.right - (21 + wCtl);
		const int yCtl = (lineCount * (hCtl + 7)) + 14;
		const int wLbl = 200; //Does not matter; will get resized
		const int xLbl = 10; //Ditto

		struct VarValue VarCurr = Var_GetValue_JSON(VarCurr_JSON, ModUUID);
		
		//Skip if mod is public
		if(strcmp(VarCurr.publicType, "") == 0){
			continue;
		}
		
		//Map hCtl to proper size
		//Below code is for actual, proper DPI compliance.
		//For consistency, we instead use font size.
		{
			/*HDC hdc = GetDC(NULL);
			int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
			double tempH = (20.0 / 96);
			hCtl = tempH * dpiY;
			yCtl = (lineCount * (hCtl + 7))+14;
			ReleaseDC(NULL, hdc);*/
		}
		
		//Generate label
		//If I weren't lazy, I'd draw this with GDI.
		//Unfortunately, this is [CURRENTYEAR], so I won't.
		//(Also, I wrote this in C for Windows 95. Don't complain
		//about laziness now. I'm *dedicated*, man.)
		CreateWindowEx(
			0, "Static", VarCurr.desc,
			WS_CHILD | WS_VISIBLE,
			xLbl, yCtl,
			wLbl, hCtl,
			VarWin,
			(HMENU)IDC_STATIC, //Unique numeric identifier
			CURRINSTANCE, NULL
		);
		
		//Determine type of control and create it
		if(
			(
				strcmp(VarCurr.publicType, "Numeric") == 0 ||
				strcmp(VarCurr.publicType, "HexNum") == 0
			) && (
				VarCurr.type != uInt32 &&
				VarCurr.type != Int32 &&
				VarCurr.type != IEEE64 &&
				VarCurr.type != IEEE32
			)
		){
			HWND hNumControl;
			HWND hSpinControl;			
					
			//Create textbox
			hNumControl = CreateWindowEx(
				WS_EX_CLIENTEDGE,
				"EDIT", "0",
				WS_CHILD | WS_VISIBLE,
				xCtl, yCtl,
				wCtl - 16, hCtl,
				VarWin,
				NULL,
				CURRINSTANCE, NULL
			);
			
			//Set UUID as window property
			SetProp(hNumControl, "VarUUID", strdup(VarCurr.UUID));
			SetProp(hNumControl, "HasBuddy", "True");
			
			///Create up/down
			hSpinControl = CreateWindow(
				UPDOWN_CLASS, NULL,
				UDS_ALIGNRIGHT | UDS_ARROWKEYS | WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT,
				xCtl, yCtl,
				wCtl, hCtl,
				VarWin,
				NULL,
				CURRINSTANCE, NULL
			);
						
			//Set textbox to be buddy of up/down
			//to let textbox show contents of spinbox
			SendMessage(hSpinControl, UDM_SETBUDDY, (WPARAM)hNumControl, 0);
			
			if(strcmp(VarCurr.publicType, "HexNum") == 0){
				char *NumText = NULL;
				
				//Set control type as window property
				SetProp(hNumControl, "CtlType", "HexNum");
				
				//Hex view only supports unsigned values.
				//Set range/value to whatever it's saved as
				switch(VarCurr.type){
				default:
				case Int16:
				case uInt16:
					SendMessage(hSpinControl, UDM_SETRANGE, 0, MAKELONG(USHRT_MAX, 0));
					SendMessage(hSpinControl, UDM_SETPOS, 0, MAKELONG(VarCurr.uInt16, 0));
					asprintf(&NumText, "0x%04hX", VarCurr.uInt16);
					break;
				case Int8:
				case uInt8:
					SendMessage(hSpinControl, UDM_SETRANGE, 0, MAKELONG(UCHAR_MAX, 0));
					SendMessage(hSpinControl, UDM_SETPOS, 0, MAKELONG(VarCurr.uInt8, 0));
					asprintf(&NumText, "0x%02hhX", VarCurr.uInt8);
					break;
				}
				
				//Set spinner mode to hex
				SendMessage(hSpinControl, UDM_SETBASE, 16, 0);
				
				//Set numbox text
				SetWindowText(hNumControl, NumText);
				safe_free(NumText);
				
			} else {
				char *NumText = NULL;
				
				//Set control type as window property
				SetProp(hNumControl, "CtlType", "Numeric");
				
				//Set range depending on type
				switch(VarCurr.type){
				default:
				case Int16:
					SendMessage(hSpinControl, UDM_SETRANGE, 0, MAKELONG(USHRT_MAX, 0));
					SendMessage(hSpinControl, UDM_SETPOS, 0, MAKELONG(VarCurr.Int16, 0));
					asprintf(&NumText, "%hu", VarCurr.uInt16);
					break;
				case uInt16:
					SendMessage(hSpinControl, UDM_SETRANGE, 0, MAKELONG(USHRT_MAX, 0));
					SendMessage(hSpinControl, UDM_SETPOS, 0, MAKELONG(VarCurr.uInt16, 0));
					asprintf(&NumText, "%hd", VarCurr.uInt16);
					break;
				case Int8:
					SendMessage(hSpinControl, UDM_SETRANGE, 0, MAKELONG(UCHAR_MAX, 0));
					SendMessage(hSpinControl, UDM_SETPOS, 0, MAKELONG(VarCurr.Int8, 0));
					asprintf(&NumText, "%hhu", VarCurr.uInt16);
					break;
				case uInt8:
					SendMessage(hSpinControl, UDM_SETRANGE, 0, MAKELONG(UCHAR_MAX, 0));
					SendMessage(hSpinControl, UDM_SETPOS, 0, MAKELONG(VarCurr.uInt8, 0));
					asprintf(&NumText, "%hhd", VarCurr.uInt16);
					break;
				}
				
				//Set numbox text
				SetWindowText(hNumControl, NumText);
				safe_free(NumText);
			}
			
		} else if (
			strcmp(VarCurr.publicType, "Numeric") == 0 ||
			strcmp(VarCurr.publicType, "HexNum") == 0
		){
			//Number out of spinbox range; omit spinbox
			HWND hNumControl;
			char *NumText = NULL;
					
			//Create textbox
			hNumControl = CreateWindowEx(
				WS_EX_CLIENTEDGE,
				"EDIT", "0",
				WS_CHILD | WS_VISIBLE,
				xCtl, yCtl,
				wCtl, hCtl,
				VarWin,
				NULL,
				CURRINSTANCE, NULL
			);
			
			//Set window properties
			SetProp(hNumControl, "VarUUID", strdup(VarCurr.UUID));
			SetProp(hNumControl, "CtlType", "Numeric");
			
			switch(VarCurr.type){
			default:
			case Int32:
				asprintf(&NumText, "%d", VarCurr.Int32);
			break;
			
			case uInt32:
				asprintf(&NumText, "%u", VarCurr.uInt32);
			break;
			
			case IEEE32:
				asprintf(&NumText, "%f", VarCurr.IEEE32);
			break;
			
			case IEEE64:
				asprintf(&NumText, "%f", VarCurr.IEEE64);
			break;
			}
			SetWindowText(hNumControl, NumText);
			safe_free(NumText);
			
			
		} else if (strcmp(VarCurr.publicType, "Checkbox") == 0){
			HWND hChkControl;
					
			//Create textbox
			hChkControl = CreateWindow(
				"BUTTON", "",
				WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
				xCtl, yCtl,
				wCtl, hCtl,
				VarWin,
				NULL,
				CURRINSTANCE, NULL
			);
			
			//Set window properties
			SetProp(hChkControl, "VarUUID", strdup(VarCurr.UUID));
			SetProp(hChkControl, "CtlType", "Checkbox");
			SendMessage(
				hChkControl,
				BM_SETCHECK,
				VarCurr.uInt32 ? BST_UNCHECKED : BST_CHECKED,
				0
			);
			
		} else if (strcmp(VarCurr.publicType, "List") == 0){
			HWND hListBox; //Actually a combo box. Eh.
			int i;
			long BoxSize, newHeight;
			
			hListBox = Dlg_Var_CreateListBox(VarWin, VarCurr.UUID);
			
			//Resize window to proper place.
			newHeight = hCtl * (SendMessage(hListBox, CB_GETCOUNT, 0, 0) + 1);
			MoveWindow(hListBox, xCtl - 10, yCtl, wCtl + 10, newHeight, TRUE);
			
			//Set window properties
			SetProp(hListBox, "VarUUID", strdup(VarCurr.UUID));
			SetProp(hListBox, "CtlType", "List");
			
			//Set current selection by searching for contained data
			BoxSize = SendMessage(hListBox, CB_GETCOUNT, 0, 0);
			for(i = 0; i < BoxSize; i++){
				long FoundData = SendMessage(hListBox, CB_GETITEMDATA, i, 0);
				if(VarCurr.Int32 == FoundData){
					SendMessage(hListBox, CB_SETCURSEL, i, 0);
					break;
				}
			}
		}
		
		//Free junk in those variables
		safe_free(VarCurr.UUID);
		safe_free(VarCurr.desc);
		safe_free(VarCurr.publicType);
		safe_free(VarCurr.mod);
		lineCount += 1;
	}
	
	//Record line count
	{
		int *LineCountPtr = NULL;
		LineCountPtr = malloc(sizeof(int));
		*LineCountPtr = lineCount;
		SetProp(VarWin, "LineCount", LineCountPtr);
	}
	
	//Set scroll bar
	{
		SCROLLINFO VarWinScroll;
		VarWinScroll.cbSize = sizeof(SCROLLINFO);
		VarWinScroll.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_DISABLENOSCROLL;
		
		VarWinScroll.nMin = 0;
		//VarWinScroll.nMax = (lineCount * (DLGFONTHEIGHT + 7)) + 14;
		VarWinScroll.nMax = (lineCount * (DLGFONTHEIGHT + 14)) + 14;
		VarWinScroll.nPage = VarWinSize.bottom;
		VarWinScroll.nPos = 0;
		
		SetScrollInfo(VarWin, SB_VERT, &VarWinScroll, TRUE);
	}
	
	//Add OK and Cancel buttons in new window
	CreateWindow(
		"BUTTON", "OK",
		WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
		BigWinSize.right - (75 + 14),
		VarWinSize.bottom + 7,
		75, 23,
		BigWin,
		(HMENU)IDOK,
		CURRINSTANCE, NULL
	);
	
	CreateWindow(
		"BUTTON", "Cancel",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		BigWinSize.right - (75 + 14) - (75 + 4),
		VarWinSize.bottom + 7,
		75, 23,
		BigWin,
		(HMENU)IDCANCEL,
		CURRINSTANCE, NULL
	);
		
	//Set window font (so it's not Windows 3.1's default.)
	SetFont(BigWin, (LPARAM)DLGFONT);
	EnumChildWindows(BigWin, (WNDENUMPROC)SetFont, (LPARAM)DLGFONT);
	
	//Resize window
	{
		RECT ClientRect;
		GetClientRect(BigWin, &ClientRect);
		SendMessage(BigWin, WM_SIZE, 0, MAKELONG(ClientRect.right, ClientRect.bottom));
	}
	
	//Return, finally
	return BigWin;
}

// Generic Dialog Procedure
INT_PTR CALLBACK Dlg_Generic(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{	
	switch(Message){
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		case IDOK:
			EndDialog(hwnd, IDOK);
			break;
		}
	break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

//Profile Dialog Procedure
INT_PTR CALLBACK Dlg_Profile(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{	
	switch(Message){
	case WM_INITDIALOG:{
		struct ProgConfig *LocalConfig;
		
		//Make internal copy
		LocalConfig = malloc(sizeof(struct ProgConfig));
		
		//Load configuration, if it exists
		if(!CONFIG.CURRPROF){
			//First run
			//Keep all fields blank
			memset(LocalConfig, 0, sizeof(*LocalConfig));
			LocalConfig->PROGDIR = strdup(CONFIG.PROGDIR);
			LocalConfig->CURRPROF = strdup("");
		} else {
			//Load profile!
			safe_free(LocalConfig);
			LocalConfig = Profile_Load(CONFIG.CURRPROF);
		}
		
		//Save internal copy pointer w/ window
		SetProp(hwnd, "LocalConfig", LocalConfig);
		
		//Reload dialog
		SendMessage(hwnd, WMX_RELOADPROF, 0, 0);
	break;
	}
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDCANCEL:
			//We didn't change CONFIG, so just close
			EndDialog(hwnd, IDCANCEL);
			break;
			
		case IDOK:{
			//Save profile
			struct ProgConfig *LocalConfig = GetProp(hwnd, "LocalConfig");
			if (
				!LocalConfig->CURRPROF ||
				strcmp(LocalConfig->CURRPROF, "") == 0
			) {
				AlertMsg("You must create or load a profile.", "No profile");
				break;
			} else if(
				!LocalConfig->CURRDIR ||
				strcmp(LocalConfig->CURRDIR, "") == 0
			){
				AlertMsg("You must provide the location of a game.", "No game path");
				break;
			}

			//Pull RUNPATH from the dialog box as that's the only way to edit it.
			safe_free(LocalConfig->RUNPATH);
			LocalConfig->RUNPATH = GetWindowTextMalloc(GetDlgItem(hwnd, IDC_CFGRUNPATH));
			
			//Save!
			Profile_DumpLocal(LocalConfig);
			
			//Copy LocalConfig to CONFIG
			Profile_EmptyStruct(&CONFIG);
			memcpy(&CONFIG, LocalConfig, sizeof(CONFIG));
				
			EndDialog(hwnd, IDOK);
			break;
		}
		case IDC_CFGPROFBROWSE:{
			//Get path to a profile
			json_t *tempProf;
			struct ProgConfig *LocalConfig = GetProp(hwnd, "LocalConfig");
			int nameOffset; //unused
			char *fpath = SelectFile(
				hwnd, &nameOffset,
				"SrModLdr Profile (.json)\0*.json\0\0"
			);
			
			//If no path given, quit
			if(fpath == NULL || strcmp(fpath, "") == 0){
				break;
			}
			
			//Verify path 
			tempProf = JSON_Load(fpath);
			if(!tempProf){
				//Invalid file
				safe_free(fpath);
				break;
			}
			json_decref(tempProf);
			
			//Load profile!
			Profile_EmptyStruct(LocalConfig);
			safe_free(LocalConfig);
			LocalConfig = Profile_Load(fpath);
			SetProp(hwnd, "LocalConfig", LocalConfig);
			
			//Reload dialog
			SendMessage(hwnd, WMX_RELOADPROF, 0, 0);
			break;
		}
		
		//Generate new, blank profile
		case IDC_CFGPROFNEW:{
			struct ProgConfig *LocalConfig = GetProp(hwnd, "LocalConfig");
			//Get path to a profile
			char *fpath = SaveFile(
				hwnd,
				"SrModLdr Profile (.json)\0*.json\0\0",
				"json"
			);
			
			//If no path given, quit
			if(fpath == NULL || strcmp(fpath, "") == 0){
				break;
			}
			
			//Blank LocalConfig
			Profile_EmptyStruct(LocalConfig);
			LocalConfig->PROGDIR = strdup(CONFIG.PROGDIR);
			
			//Record path
			LocalConfig->CURRPROF = fpath;
			
			//Reload dialog
			SendMessage(hwnd, WMX_RELOADPROF, 0, 0);
			break;
		}
		
		//Browse for game dir
		case IDC_CFGGAMEBROWSE:{
			char *cfgpath = NULL;
			char *fpath = NULL;
			char *GameEXE = NULL;
			char *GameVer = NULL;
			char *GameUUID = NULL;
			struct ProgConfig *LocalConfig = GetProp(hwnd, "LocalConfig");
			
			fpath = SelectFolder("Select Game Install Path");
			
			//Figure out what game type it's using
			cfgpath = Profile_FindMetadata(fpath);
			if(!cfgpath){
				AlertMsg("Could not find any known game in that folder!",
					"Game not found");
				break;
			}

			//Get default RUN path
			GameEXE = Profile_GetGameEXE(cfgpath);

			//Get game version
			GameVer = Profile_GetGameVerID(cfgpath);
			
			//Get game UUID
			GameUUID = Profile_GetGameUUID(cfgpath);
			
			//Save new config
			safe_free(LocalConfig->CURRDIR); 
			LocalConfig->CURRDIR = fpath;
			
			safe_free(LocalConfig->GAMECONFIG);
			LocalConfig->GAMECONFIG = cfgpath;
			
			safe_free(LocalConfig->RUNPATH);
			LocalConfig->RUNPATH = GameEXE;

			safe_free(LocalConfig->GAMEVER);
			LocalConfig->GAMEVER = GameVer;
			
			safe_free(LocalConfig->GAMEUUID);
			LocalConfig->GAMEUUID = GameUUID;
			
			LocalConfig->CHECKSUM = crc32File(GameEXE);
			
			//Reload dialog
			SendMessage(hwnd, WMX_RELOADPROF, 0, 0);
			break;
		}
		}
	break;
	
	//Clears profile dialog
	case WMX_BLANKPROF:
		SetWindowText(GetDlgItem(hwnd, IDC_CFGPROFPATH), "");
		UpdateWindow(GetDlgItem(hwnd, IDC_CFGPROFPATH));

		SetWindowText(GetDlgItem(hwnd, IDC_CFGGAMEPATH), "");
		UpdateWindow(GetDlgItem(hwnd, IDC_CFGGAMEPATH));

		SetWindowText(GetDlgItem(hwnd, IDC_CFGGAMENAME), "");
		UpdateWindow(GetDlgItem(hwnd, IDC_CFGGAMENAME));

		SetWindowText(GetDlgItem(hwnd, IDC_CFGGAMEVER), "");
		UpdateWindow(GetDlgItem(hwnd, IDC_CFGGAMEVER));

		SetWindowText(GetDlgItem(hwnd, IDC_CFGRUNPATH), "");
		UpdateWindow(GetDlgItem(hwnd, IDC_CFGRUNPATH));
	break;
	
	
	//Reloads profile dialog from whatever is in memory
	case WMX_RELOADPROF:{
		char *GameName = NULL;
		char *GameVer = NULL;
		struct ProgConfig *LocalConfig = GetProp(hwnd, "LocalConfig");
		if(
			!LocalConfig->CURRDIR || !LocalConfig->GAMECONFIG || 
			strcmp(LocalConfig->CURRDIR, "") == 0 ||
			strcmp(LocalConfig->GAMECONFIG, "") == 0
		){
			//Profile is empty
			char *TempProf = strdup(LocalConfig->CURRPROF);
			Profile_EmptyStruct(LocalConfig);
			LocalConfig->PROGDIR = strdup(CONFIG.PROGDIR);
			LocalConfig->CURRPROF = TempProf;
			
			//Clear window contents
			SendMessage(hwnd, WMX_BLANKPROF, 0, 0);
			SetWindowText(GetDlgItem(hwnd, IDC_CFGPROFPATH), LocalConfig->CURRPROF);

			//Redraw window
			UpdateWindow(GetDlgItem(hwnd, IDC_CFGPROFPATH));
			UpdateWindow(GetDlgItem(hwnd, IDC_CFGGAMEPATH));		
			UpdateWindow(GetDlgItem(hwnd, IDC_CFGGAMENAME));
			UpdateWindow(GetDlgItem(hwnd, IDC_CFGGAMEVER));
			UpdateWindow(GetDlgItem(hwnd, IDC_CFGRUNPATH));
			break;
		}
		
		//Get game name and version
		GameVer = Profile_GetGameVer(LocalConfig->GAMECONFIG);
		GameName = Profile_GetGameName(LocalConfig->GAMECONFIG);
		
		//Set window text
		SetWindowText(GetDlgItem(hwnd, IDC_CFGGAMENAME), GameName);
		SetWindowText(GetDlgItem(hwnd, IDC_CFGGAMEVER), GameVer);
		SetWindowText(GetDlgItem(hwnd, IDC_CFGGAMEPATH), LocalConfig->CURRDIR);
		SetWindowText(GetDlgItem(hwnd, IDC_CFGPROFPATH), LocalConfig->CURRPROF);
		SetWindowText(GetDlgItem(hwnd, IDC_CFGRUNPATH), LocalConfig->RUNPATH);

		//Redraw window
		UpdateWindow(GetDlgItem(hwnd, IDC_CFGPROFPATH));
		UpdateWindow(GetDlgItem(hwnd, IDC_CFGGAMEPATH));		
		UpdateWindow(GetDlgItem(hwnd, IDC_CFGGAMENAME));
		UpdateWindow(GetDlgItem(hwnd, IDC_CFGGAMEVER));
		UpdateWindow(GetDlgItem(hwnd, IDC_CFGRUNPATH));

		safe_free(GameVer);
		safe_free(GameName);
	break;
	}
	
	case WM_CLOSE:{
		//Free local config
		struct ProgConfig *LocalConfig = GetProp(hwnd, "LocalConfig");
		safe_free(LocalConfig);
		
		//End dialog
		DestroyWindow(hwnd);
	break;
	}
	
	default:
		return FALSE;
	}
	return TRUE;
}

// About Dialog Procedure
INT_PTR CALLBACK Dlg_About(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{	
	switch(Message){
	case WM_INITDIALOG:{
		//Rewrite copyright screen with build info
		char *CopyText = NULL;
		char *CompilerInfo = NULL;
		
		#ifdef _MSC_VER
			asprintf(
				&CompilerInfo,
				"Microsoft Visual C++ %d.%d",
				(_MSC_VER / 100) - 6, _MSC_VER % 100
			);
		#elif __BORLANDC__
			asprintf(
				&CompilerInfo,
				"Borland C++ %x.%x",
				__BORLANDC__ >> 8, __BORLANDC__ & 0xFF
			);
		#elif __GNUC__
			asprintf(
				&CompilerInfo,
				"GNU Compiler Collection %d.%d.%d",
				__GNUC__, __GNUC_MINOR__,
				__GNUC_BUGFIX__ ? __GNUC_BUGIX__ : 0
			);
		#else
			CompilerInfo = strdup("an unknown compiler.");
		#endif
		
		asprintf(
			&CopyText,
			"Sonic R Mod Loader %d.%d.%d\r\n"
			"(c) 2015-2016 InvisibleUp\r\n\r\n"
			
			"This program is not affiliated by Sega Corporation, "
			"TT Games or any subsidiaries thereof.\r\n\r\n"
			
			"Source available at\r\n"
			"%s\r\n"
			"under the Mozilla Public License 2.0\r\n\r\n"

			"Built on %s %s\r\n"
			"using %s",
			PROGVERSION_MAJOR, PROGVERSION_MINOR, PROGVERSION_BUGFIX,
			PROGSITE, __DATE__, __TIME__, CompilerInfo
		);
		SetWindowText(GetDlgItem(hwnd, IDC_ABOUTCOPYTEXT), CopyText);
		
		safe_free(CompilerInfo);
		safe_free(CopyText);
	}
	break;
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		case IDOK:
			EndDialog(hwnd, IDOK);
			break;
		}
	break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

// Main Dialog Procedure
char *Mod_GetUUIDFromWin(HWND hwnd)
{
	char *ModName = NULL;
	char *ModUUID = NULL;
	long len, CurrSel;
	HWND list;
	
	//Get the handle for the mod list
	list = GetDlgItem(hwnd, IDC_MAINMODLIST);
	
	//Get the current selection
	CurrSel = SendMessage(list, LB_GETCURSEL, 0, 0);
	if(CurrSel == -1){return NULL;} //No error -- nothing selected
	
	//Get the length of the name of the selected mod
	len = SendMessage(list, LB_GETTEXTLEN, CurrSel, 0);
	
	//Allocate space for the name
	ModName = malloc(len+1);
	if(ModName == NULL){
		AlertMsg("Malloc Fail #2", "");
		CURRERROR = errCRIT_MALLOC;
		return NULL;
	}
	
	//Actually get the name
	SendMessage(list, LB_GETTEXT, CurrSel, (LPARAM)ModName);
	
	//Use that to get the UUID
	{
		sqlite3_stmt *command = NULL;
		char *query = "SELECT UUID FROM Mods WHERE Name = ?;";
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL) ||
			sqlite3_bind_text(command, 1, ModName, -1, SQLITE_TRANSIENT)
		) != 0){
			safe_free(ModName);
			CURRERROR = errCRIT_DBASE;
			return NULL;
		}
		
		ModUUID = SQL_GetStr(command);
		
		if(ModUUID == NULL){
			safe_free(ModName);
			CURRERROR = errCRIT_DBASE;
			return NULL;
		}
		sqlite3_finalize(command);
		command = NULL;
	}	
	safe_free(ModName);
	return ModUUID;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  UpdateModList
 *  Description:  Given a handle to a ListBox, populates it with the titles of
 *                every mod loaded into the database.
 * =====================================================================================
 */
BOOL UpdateModList(HWND list)
{
	sqlite3_stmt *command = NULL;
	const char commandstr[] = "SELECT Name FROM Mods";
	json_t *out, *row;
	unsigned int i;
	CURRERROR = errNOERR;

	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, commandstr, -1, &command, NULL)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	
	out = SQL_GetJSON(command);
	if(CURRERROR != errNOERR){return FALSE;}
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}
	command = NULL;
	
	if(out == NULL){CURRERROR = errCRIT_FUNCT; return FALSE;}
	
	//Clear list
	SendMessage(list, LB_RESETCONTENT, 0, 0);
	
	//Assemble and write out the names
	json_array_foreach(out, i, row){
		char *name = NULL; 
		//Phantom entries make this angry.
		name = JSON_GetStr(row, "Name");
		if(strcmp(name, "") == 0){safe_free(name); continue;}
		SendMessage(list, LB_ADDSTRING, 0, (LPARAM)name);
		safe_free(name);
	}
	json_decref(out);
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  UpdateModDetails
 *  Description:  Updates the list of mod details (name, author, etc.) displayed
 *                on the sidebar of the program.
 * =====================================================================================
 */
BOOL UpdateModDetails(HWND hwnd, int listID)
{
	sqlite3_stmt *command = NULL;
	const char query[] = "SELECT * FROM Mods WHERE RowID = ?;";
	json_t *out, *row;
	
	BOOL noResult = FALSE;
	CURRERROR = errNOERR;
	
	//Select the right mod
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_int(command, 1, listID+1)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	
	out = SQL_GetJSON(command);
	if(CURRERROR != errNOERR){
		return FALSE;
	}
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	command = NULL;
	
	if(!out || !json_is_array(out)){
		CURRERROR = errCRIT_FUNCT;
		return FALSE;
	}
	row = json_array_get(out, 0);
	
	//Check if array is empty (if there's anything to show)
	if(!row || !json_is_object(row)){
		noResult = TRUE;
	} //How?

	if(noResult == FALSE){
		char *name = NULL, *auth = NULL, *cat = NULL;
		char *desc = NULL, *ver = NULL, *UUID = NULL;
		//Update window text from output of SQL
		name = JSON_GetStr(row, "Name");
		auth = JSON_GetStr(row, "Auth");
		cat = JSON_GetStr(row, "Cat");
		desc = JSON_GetStr(row, "Desc");
		ver = JSON_GetStr(row, "Ver");
		UUID = JSON_GetStr(row, "UUID");
		
		SetWindowText(GetDlgItem(hwnd, IDC_MAINNAME), name);
		SetWindowText(GetDlgItem(hwnd, IDC_MAINAUTHOR), auth);
		SetWindowText(GetDlgItem(hwnd, IDC_MAINTYPE), cat);
		SetWindowText(GetDlgItem(hwnd, IDC_MAINDESC), desc);
		SetWindowText(GetDlgItem(hwnd, IDC_MAINVERSION), ver);
		
		safe_free(auth);
		safe_free(cat);
		safe_free(desc);
		safe_free(ver);

		//Get used EXE and Disk space
/*		if(strcmp(name, "") != 0){
			char *EXESpc = NULL;//, *DskSpc = NULL;
			char *UUID = JSON_GetStr(row, "UUID");
			asprintf(&EXESpc, "%d bytes", GetUsedSpaceBytes(UUID, 1));
			//asprintf(&DskSpc, "%d bytes", GetDiskSpaceBytes(UUID));
			
			//Write to dialog
			SetWindowText(GetDlgItem(hwnd, IDC_MAINEXESPACE), EXESpc);
			safe_free(EXESpc);
		} else {
			SetWindowText(GetDlgItem(hwnd, IDC_MAINEXESPACE), "");
		}*/
		
		//Load bitmap if applicable
		{
			char *modPath = NULL;
			char *bitmapPath = NULL;	
			
			//Get mod path
			const char *query = "SELECT Path FROM Mods WHERE UUID = ?";
			sqlite3_stmt *command = NULL;
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT)
			) != 0 ){
				CURRERROR = errCRIT_DBASE; return FALSE;
			}
			modPath = SQL_GetStr(command);
			sqlite3_finalize(command);
			command = NULL;
			safe_free(UUID);
			
			//Get bitmap path
			asprintf(&bitmapPath, "%s\\preview.bmp", modPath);
			safe_free(modPath);
			
			//Check if path is OK
			if(access(bitmapPath, F_OK) == 0){
				//Create bitmap
				prevBitmap = LoadImage(
					CURRINSTANCE, bitmapPath, IMAGE_BITMAP,
					0, 0, LR_LOADFROMFILE 
				);
				//Draw bitmap
				ShowWindow(GetDlgItem(hwnd, IDC_MAINPREVIEW), TRUE);
				InvalidateRect(hwnd, NULL, FALSE);
			} else {
				//Create dummy bitmap
				prevBitmap = CreateBitmap(0, 0, 1, 1, NULL);
				//Hide window
				ShowWindow(GetDlgItem(hwnd, IDC_MAINPREVIEW), FALSE);
			}

			safe_free(bitmapPath);
		}

		safe_free(UUID);
		safe_free(name);
		json_decref(out);

	} else {
		SetWindowText(GetDlgItem(hwnd, IDC_MAINNAME), "");
		SetWindowText(GetDlgItem(hwnd, IDC_MAINAUTHOR), "");
		SetWindowText(GetDlgItem(hwnd, IDC_MAINTYPE), "");
		SetWindowText(GetDlgItem(hwnd, IDC_MAINDESC), "");
		SetWindowText(GetDlgItem(hwnd, IDC_MAINVERSION), "");
//		SetWindowText(GetDlgItem(hwnd, IDC_MAINEXESPACE), "");

		prevBitmap = CreateBitmap(0, 0, 1, 1, NULL);
		ShowWindow(GetDlgItem(hwnd, IDC_MAINPREVIEW), FALSE);
	}

	return TRUE;
}

BOOL Mod_InstallPrep(HWND hwnd)
{
	json_t *jsonroot;
	char *fpath = NULL; // Path mod is located at
	char *fname = NULL; // Filename of mod
	char *tempdir = NULL;
	char *temppath = NULL;
	char *destpath = NULL;
	char *destjson = NULL;
	int nameOffset;
	BOOL retval = TRUE;
	char *upgradeList = NULL;
	char *UUID = NULL;
	
	CURRERROR = errNOERR;

	//Select mod
	fpath = SelectFile(hwnd, &nameOffset, "Mod Package (.zip)\0*.zip\0\0");
	//If no path given, quit
	if(fpath == NULL || strcmp(fpath, "") == 0){
		if(CURRERROR == errNOERR){
			return TRUE;
		} else {
			ErrCracker(CURRERROR);
			return FALSE;
		}
	}
	//Get filename
	fname = strdup(fpath + nameOffset);
	//Strip extension (for new folder)
	{
		char *pch = strrchr(fname, '.');
		if(pch){*pch = '\0';}
	}
	
	/// Verify mod (without overwriting existing installation)
	//Set temporary directory
	tempdir = calloc(MAX_PATH, sizeof(char));
	GetTempPath(MAX_PATH, tempdir);
	asprintf(&temppath,  "%s%s\\", tempdir, fname);
	asprintf(&destjson, "%s%s", temppath, "info.json");
	
	//Extract to that directory
	if(extract(fpath, temppath) == FALSE){
		AlertMsg("ZIP error!", "DEBUG");
		CURRERROR = errWNG_MODCFG;
		jsonroot = json_object(); //Might be important?
		goto Mod_InstallPrep_Cleanup;
	}
	
	//Load
	jsonroot = JSON_Load(destjson);
	if (jsonroot == NULL){
		AlertMsg(
			"Could not load 'info.json' in given ZIP.",
			"Mod Config Error"
		);
		CURRERROR = errWNG_MODCFG;
		jsonroot = json_object(); //Might be important?
		goto Mod_InstallPrep_Cleanup;
	}
	//Retrive UUID
	UUID = JSON_GetStr(jsonroot, "UUID");
	
	//Verify
	if(!Mod_Verify(jsonroot)){
		//Delete temp directory
		//AlertMsg(temppath, "debug: LAST CHANCE SERIOUSLY");
		File_DelTree(temppath);
		goto Mod_InstallPrep_Cleanup;
	}
	if(CURRERROR == errUSR_CONFIRM){
		//Oooh, we need to replace
		upgradeList = Mod_UninstallSeries(UUID);
	}

	//Get path for destination
	fname = Mod_MangleUUID(UUID);

	//Unload
	json_decref(jsonroot);
	jsonroot = NULL;
	
	/// Copy mod to [path]/mods/[name]
	//Set destination
	asprintf(&destpath, "%s\\mods\\%s\\", CONFIG.CURRDIR, fname);
	asprintf(&destjson, "%s%s", destpath, "info.json");
	
	//Delete destination if exists
//	if(access(destpath, R_OK)){
//		AlertMsg(destpath, "debug: LAST CHANCE SERIOUSLY");
//		File_DelTree(destpath);
//	}
	
	//Move directory
/*	if(File_MovTree(temppath, destpath) != 0){
		//... but how?
		DWORD err = errno;
		CURRERROR = errCRIT_FUNCT;
		goto Mod_InstallPrep_Cleanup;
	}*/
	
	//Load (again)
	jsonroot = JSON_Load(destjson);
	if (jsonroot == NULL){
		AlertMsg(
			"Could not load info.json in given ZIP.",
			"Mod Config Error"
		);
		CURRERROR = errWNG_MODCFG;
		jsonroot = json_object(); //Might be important?
		goto Mod_InstallPrep_Cleanup;
	}
	
	///Install mod
	//Disable window during install
	EnableWholeWindow(hwnd, FALSE);
	
	if (Mod_Install(jsonroot, destpath) == FALSE){
		char *ModUUID = NULL;
		
		ModUUID = JSON_GetStr(jsonroot, "UUID");
		//AlertMsg("Installation failed. Rolling back...", "Installation Failure");
		Mod_Uninstall(ModUUID);
		safe_free(ModUUID);
		
		retval = FALSE;
		goto Mod_InstallPrep_Cleanup;
	};
	
	//Reinstall previous mods if needed
	if(upgradeList){
		Mod_InstallSeries(upgradeList);
		safe_free(upgradeList);
	}
	
	//Update mod list
	UpdateModList(GetDlgItem(hwnd, IDC_MAINMODLIST));
	UpdateModDetails(
		hwnd, 
		SendMessage(
			GetDlgItem(hwnd, IDC_MAINMODLIST),
			LB_GETCURSEL, 0, 0
		)
	);
	
	//Re-enable window & Cleanup
Mod_InstallPrep_Cleanup:
	safe_free(UUID);
	safe_free(fpath);
	safe_free(destpath);
	safe_free(destjson);
	safe_free(temppath);
	safe_free(tempdir);
	EnableWholeWindow(hwnd, TRUE);
	json_decref(jsonroot);
	return retval;
}

BOOL Mod_UninstallPrep(HWND hwnd){
	//Get the UUID of the currently selected mod
	char *ModUUID = Mod_GetUUIDFromWin(hwnd);
	char *oldMods = NULL;
	if(ModUUID == NULL){return FALSE;}
	
	//Finally remove the mod
	EnableWholeWindow(hwnd, FALSE);
	oldMods = Mod_UninstallSeries(ModUUID);
	//ErrCracker(CURRERROR);
	safe_free(ModUUID);

	//Readd old mods
	Mod_InstallSeries(oldMods);
	
	//Update UI
	UpdateModList(GetDlgItem(hwnd, IDC_MAINMODLIST));
	UpdateModDetails(hwnd, SendMessage(
		GetDlgItem(hwnd, IDC_MAINMODLIST),
		LB_GETCURSEL, 0, 0)
	);
	//ErrCracker(CURRERROR);
	EnableWholeWindow(hwnd, TRUE);
	return TRUE;
}

INT_PTR CALLBACK Dlg_Main(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message){
	case WM_INITDIALOG:{
		//Set program icon
	/*	SendMessage(hwnd, WM_SETICON, ICON_SMALL,
			(LPARAM)LoadImage(CURRINSTANCE,
			MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0));*/
		HICON hIconSmall, hIconLarge;

		// I don't know why this doesn't work with Visual Studio.
		// Works fine in Borland.
		hIconSmall = LoadImage(
			CURRINSTANCE,
			MAKEINTRESOURCE(IDI_ICON),
			IMAGE_ICON,
			GetSystemMetrics(SM_CXSMICON),
			GetSystemMetrics(SM_CYSMICON),
			0
		);
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

		hIconLarge = LoadImage(
			CURRINSTANCE,
			MAKEINTRESOURCE(IDI_ICON),
			IMAGE_ICON,
			GetSystemMetrics(SM_CXICON),
			GetSystemMetrics(SM_CYICON),
			0
		);
		SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
			
		//Update GUI Elements
		UpdateModList(GetDlgItem(hwnd, IDC_MAINMODLIST));

		//Create dummy bitmap
		prevBitmap = CreateBitmap(0, 0, 1, 1, NULL);

		return TRUE;
	}
	case WM_COMMAND:{
		switch(LOWORD(wParam)){
		case IDCANCEL:
			DestroyWindow(hwnd);
			PostQuitMessage(0);
		break;
		
		case IDO_ABOUT:
			DialogBoxSysFont(IDD_ABOUT, Dlg_About, hwnd);
		break;
		
		case IDO_CHANGEPROF:{
			json_t *GameCfg;
			
			//Unload SQL & Game configuration
			sqlite3_close(CURRDB);
			
			//Open dialog
			DialogBoxSysFont(IDD_PROGCONFIG, Dlg_Profile, hwnd);
			
			//Reload game configuration
			chdir(CONFIG.PROGDIR);
			GameCfg = JSON_Load(CONFIG.GAMECONFIG);
			chdir(CONFIG.CURRDIR);
			
			//Reload SQL
			SQL_Load();
			SendMessage(hwnd, WMX_ERROR, 0, 0);
			
			SQL_Populate(GameCfg);
			SendMessage(hwnd, WMX_ERROR, 0, 0);
			json_decref(GameCfg);
			
			//Update main dialog
			UpdateModList(GetDlgItem(hwnd, IDC_MAINMODLIST));
		break;
		}
		
		/*case IDC_MAINEXEMORE:
			// Call Patch dialog with argument set to file ID 0 (the .EXE)
			DialogBoxParam(GetModuleHandle(NULL),
				MAKEINTRESOURCE(IDD_PATCHES), hwnd, Dlg_Patch, 0);
		break;
		
		case IDC_MAINDISKMORE:
			DialogBox(GetModuleHandle(NULL),
				MAKEINTRESOURCE(IDD_DISK), hwnd, Dlg_Generic);
		break;*/
		
		case IDLOAD:
			Mod_InstallPrep(hwnd);
		break;
		case IDREMOVE:
			Mod_UninstallPrep(hwnd);
		break;
		
		case IDC_MAINMODLIST:
			switch(HIWORD(wParam)){
			case LBN_SELCHANGE:
				UpdateModDetails(hwnd, SendMessage(
					GetDlgItem(hwnd, IDC_MAINMODLIST),
					LB_GETCURSEL, 0, 0)
				);
			break;
			}
		break;
		
		case IDC_MAINVARS:{
			HWND VarWin = NULL;
			char *UUID = NULL;
			
			//Get Mod UUID
			UUID = Mod_GetUUIDFromWin(hwnd);
			if(!UUID){break;} //In case nothing is selected
			
			//Generate window
			VarWin = Var_GenWindow(UUID, hwnd);
			if(!VarWin){break;}
			
			//Show window
			ShowWindow(VarWin, SW_SHOWNORMAL);
		break;
		}
		
		case IDC_MAINRUN:{
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
			
			//Block input
			EnableWholeWindow(hwnd, FALSE);
			
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
						// Note that if your main message loop does additional processing
						// (such as calling IsDialogMessage() for modeless dialogs)
						// you will want to do those things here, too.
					}
					break;
				}
			}
			
			//Reenable window
			EnableWholeWindow(hwnd, TRUE);
			
		break;
		}
		
		}
		
		//Check for errors
		SendMessage(hwnd, WMX_ERROR, 0, 0);
	return TRUE;
	}
	
	case WM_PAINT:{
		PAINTSTRUCT ps;
		HDC hdc;
		BITMAP bitmap;
		HDC hdcMem;
		HGDIOBJ oldBitmap;
		RECT prevRect;
		HWND prevWin = GetDlgItem(hwnd, IDC_MAINPREVIEW);
		HBRUSH bgBrush;

		//Get rectangle for preview area and dialog
		GetClientRect(prevWin, &prevRect);
		prevRect.right -= 2; prevRect.bottom -= 2;

		//Init bitmap
		hdc = BeginPaint(prevWin, &ps);

		hdcMem = CreateCompatibleDC(hdc);
		oldBitmap = SelectObject(hdcMem, prevBitmap);

		GetObject(prevBitmap, sizeof(bitmap), &bitmap);

		//Fill in background
		bgBrush = CreateSolidBrush(GetPixel(hdcMem, 0, 0));
		SelectObject(hdc, bgBrush);
		Rectangle(hdc, prevRect.left, prevRect.top, prevRect.right, prevRect.bottom);
		DrawEdge(hdc, &prevRect, EDGE_ETCHED, BF_RECT);
		DeleteObject(bgBrush);

		//Center image
		prevRect.left += ((prevRect.right - prevRect.left) - bitmap.bmWidth) / 2;
		prevRect.right += ((prevRect.right - prevRect.left) - bitmap.bmWidth) / 2;
		prevRect.top += ((prevRect.bottom - prevRect.top) - bitmap.bmHeight) / 2;
		prevRect.bottom += ((prevRect.bottom - prevRect.top) - bitmap.bmHeight) / 2;

		//Draw image
		BitBlt(
			hdc,
			prevRect.left, prevRect.top,
			prevRect.right - prevRect.left, 
			prevRect.bottom - prevRect.top,
			hdcMem,
			0, 0, SRCCOPY
		);

		//Wrap up
		SelectObject(hdcMem, oldBitmap);
		DeleteDC(hdcMem);

		EndPaint(prevWin, &ps);

		return FALSE;
	}
	
	case WMX_ERROR:{
		ErrCracker(CURRERROR); //Just in case
	return TRUE;
	}
	
	default:
		return FALSE;
	}
	ErrCracker(CURRERROR); //Just in case
}