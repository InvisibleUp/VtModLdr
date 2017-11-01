/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//Includes and macros
#include <stdlib.h>                // Malloc and other useful things
#include <errno.h>                 // Catching and explaining errors
#include <stdarg.h>                // Indeterminate argument support
#include <assert.h>                // Debugging
#include <math.h>                  // HUGE_VAL
#include <fcntl.h>                 // Flags for open() (O_RDWR, O_RDONLY)
#include <io.h>                    // Standardized I/O library. (Portable!)
#include <dir.h>                   // Switch and manipulate directories
#include <stdint.h>                // Variable type sizes
#include <wchar.h>                 // Unicode text functions
#include <limits.h>                // Variable sizes

#ifndef nftw                       // Define stuff to walk the filetree
#include <dirent.h>                // View contents of directories
#include <sys/stat.h>              // Check if listed file is a directory
#endif

#include <windows.h>               // Win32 APIs for the frontend
#include <commctrl.h>              // Common Controls for listbox, etc.
#include <Shlobj.h>                // "Open Folder" dialog

#include <jansson.h>               // EXTERNAL (http://www.digip.org/jansson/)
                                   // JSON parser. for loading patches, etc.
                                   // and parsing SQL results
#include <sqlite3.h>               // EXTERNAL (http://sqlite.org)
                                   // Embedded database. Used for quick lookups
                                   // of existing mods, dependencies, etc.

#include "resource.h"              // LOCAL: Defines IDs to access resources
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages

#define PROGVERSION_MAJOR 1        // Major version. Compatibility-breaking.
#define PROGVERSION_MINOR 0        // Minor version. Shiny new features.
#define PROGVERSION_BUGFIX 0       // Bugfix version. You'll need this.
const long PROGVERSION =           // Program version in format 0x00MMmmbb
	PROGVERSION_BUGFIX +
	(PROGVERSION_MINOR*0x100) +
	(PROGVERSION_MAJOR*0x10000);
//Name of program's primary author and website
const char PROGAUTHOR[] = "InvisibleUp";
const char PROGSITE[] = "http://github.com/InvisibleUp/SrModLdr/";

char *CURRDIR; //Current root directory of game
sqlite3 *CURRDB; //Current database holding patches
HINSTANCE CURRINSTANCE; //Current instance of application

HFONT DLGFONT; //Default dialog font
unsigned short DLGFONTSIZE; //Size of default dialog font

//Temporary until we get a config file going
const char GAMECONFIG[] = "./games/sonicr.json";

//Error code; like errno but specific to app
enum errCode CURRERROR = errNOERR;

///Generic helper functions
#define safe_free(ptr) {free(ptr); ptr = 0;}

///Win32 helper functions
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
 *         Name:  ErrCracker
 *  Description:  Provides generic responses for various error codes
 * =====================================================================================
 */
void ErrCracker(enum errCode error)
{
	switch(error){

	//Non-errors
	case errNOERR:
	case errUSR_ABORT:
	case errUSR_QUIT:
		//No action
		break;

	//Critical errors
	case errCRIT_DBASE:
		AlertMsg(errormsg_Crit_Dbase, "Database Failure!");
		exit(-1); //Quit ASAP
		break;
	case errCRIT_FILESYS:
		AlertMsg(errormsg_Crit_Sys, "File System Failure!");
		exit(-1); //Quit ASAP
		break;
	case errCRIT_MALLOC:
		AlertMsg(errormsg_Crit_Malloc, "Memory Failure!");
		exit(-1); //Quit ASAP
		break;
	case errCRIT_FUNCT:
		AlertMsg(errormsg_Crit_Funct, "Function Failure!");
		exit(-1); //Quit ASAP
		break;
	case errCRIT_ARGMNT:
		AlertMsg(errormsg_Crit_Argmnt, "Developer Failure!");
		exit(-1); //Quit ASAP
		break;

	//Warnings
	case errWNG_NOSPC:
		AlertMsg(errormsg_Wng_NoSpc, "No Space in File!");
		break;
	case errWNG_BADDIR:
		AlertMsg(errormsg_Wng_BadDir, "Incorrect Directory!");
		break;
	case errWNG_BADFILE:
		AlertMsg(errormsg_Wng_BadFile, "Incorrect File!");
		break;
	case errWNG_CONFIG:
		AlertMsg(errormsg_Wng_Config, "Configuration File Error!");
		break;
	case errWNG_READONLY:
		AlertMsg(errormsg_Wng_ReadOnly, "File or Directory Read-Only!");
		break;
	case errWNG_MODCFG:
		AlertMsg(errormsg_Wng_ModCfg, "Mod Metadata Error!");
		break;

	//This should never get called.
	default:
		AlertMsg("An unknown error occurred.", "Unknown error");
		break;
	}
	//Clear errors
	CURRERROR = errNOERR;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  ErrNo2ErrCode
 *  Description:  Sets CURRERROR to closest equivalent of the current value of errno
 * =====================================================================================
 */
void ErrNo2ErrCode(){
	switch(errno){
	case ENAMETOOLONG: //Pathname too long (let's not crash for silliness like that)
	case ENOENT: //File/Folder does not exist
	case EBADF: //Invalid file descriptor
		CURRERROR = errWNG_BADFILE; //Assuming file. Need context.
		break;

	case ENOTDIR: //Selected thing is not a directory
		CURRERROR = errWNG_BADDIR;
		break;

	case EACCES: //Read only error
		CURRERROR = errWNG_READONLY;
		break;

	case EFAULT: //Memory address space error (segfault)
	case ENOMEM: //Kernel out of memory
		CURRERROR = errCRIT_MALLOC;
		break;

	case EIO: //Generic I/O error
		CURRERROR = errCRIT_FILESYS;
		break;

	default: //This is unsafe, but I don't have choices here
		CURRERROR = errNOERR;
		break;
	}
}

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
	//Disable all controls, but not main window.
	//This prevents window from flying into BG.
	while (hCtl) {
		EnableWindow(hCtl, state);
		hCtl = GetWindow(hCtl, GW_HWNDNEXT);
	}
	RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE || RDW_UPDATENOW || RDW_ALLCHILDREN);
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  SelectFolder
 *  Description:  Wrapper around SHBrowseForFolder that is designed to
 *                get only valid folder names (not My Computer, for instance).
 * =====================================================================================
 */
char * SelectFolder(LPCSTR title)
{
	BROWSEINFO bi = {0}; // For init directory selector
	LPITEMIDLIST pidl;
	char *path = malloc(MAX_PATH);
	CURRERROR = errNOERR;
	bi.lpszTitle = title;

	if(path == NULL){
		AlertMsg("Malloc Fail #3", "");
		CURRERROR = errCRIT_MALLOC;
		return strdup("");
	}
	strcpy(path, "");



	pidl = SHBrowseForFolder ( &bi );
	// If user hits Cancel, stop nagging user
	if (pidl == 0){
		CURRERROR = errUSR_ABORT;
		safe_free(path);
		return strdup("");
	}

	// Get the name of the folder
	if (!SHGetPathFromIDList ( pidl, path )){
		//User chose invalid folder
		CURRERROR = errWNG_BADDIR;
		safe_free(path);
		return strdup("");
	}
	return path;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  SelectFile
 *  Description:  Simple wrapper around GetOpenFileName.
 * =====================================================================================
 */
char * SelectFile(HWND hwnd, int *nameOffset, const char *Filter)
{
	OPENFILENAME ofn = {0};
	char *fpath = calloc(MAX_PATH, sizeof(char));

        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = fpath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER|OFN_ENABLESIZING|OFN_HIDEREADONLY;
        ofn.lpstrFilter = Filter;
        ofn.lpstrCustomFilter = NULL;
        ofn.nFilterIndex = 0;
        ofn.lpstrFileTitle = NULL;
        ofn.lpstrInitialDir = NULL;
        ofn.lpstrTitle = NULL;

	GetOpenFileName(&ofn);
	//TODO: Error checking?

	//Get just the filename
	if(nameOffset != NULL){
		*nameOffset = ofn.nFileOffset;
	}

	return fpath;
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

	//Get progress bar control.
	ProgBar = GetDlgItem(ProgDialog, IDC_PROGBAR);

	//Update bar
	SendMessage(ProgBar, PBM_DELTAPOS, Delta, 0);
	//Weird Aero hack so you can actually see progress. Curse you Aero.
	SendMessage(ProgBar, PBM_DELTAPOS, -1, 0);
	SendMessage(ProgBar, PBM_DELTAPOS, 1, 0);

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

	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, commandstr, -1, &command, NULL)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}

	out = SQL_GetJSON(command);
	if(CURRERROR != errNOERR){return FALSE;}

	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
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
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_int(command, 1, listID+1)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}

	out = SQL_GetJSON(command);
	if(CURRERROR != errNOERR){return FALSE;}

	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}
	command = NULL;

	if(!out || !json_is_array(out)){CURRERROR = errCRIT_FUNCT; return FALSE;}
	row = json_array_get(out, 0);

	//Check if array is empty (if there's anything to show)
	if(!row || !json_is_object(row)){noResult = TRUE;} //How?

	if(noResult == FALSE){
		char *name = NULL, *auth = NULL, *cat = NULL, *desc = NULL, *ver = NULL;
		//Update window text from output of SQL
		name = JSON_GetStr(row, "Name");
		auth = JSON_GetStr(row, "Auth");
		cat = JSON_GetStr(row, "Cat");
		desc = JSON_GetStr(row, "Desc");
		ver = JSON_GetStr(row, "Ver");

		SetWindowText(GetDlgItem(hwnd, IDC_MAINNAME), name);
		SetWindowText(GetDlgItem(hwnd, IDC_MAINAUTHOR), auth);
		SetWindowText(GetDlgItem(hwnd, IDC_MAINTYPE), cat);
		SetWindowText(GetDlgItem(hwnd, IDC_MAINDESC), desc);
		SetWindowText(GetDlgItem(hwnd, IDC_MAINVERSION), ver);

		safe_free(auth);
		safe_free(cat);
		safe_free(desc);
		safe_free(ver);
		//Still using name

		//Get used EXE space
		if(strcmp(name, "") != 0){
			char *EXESpc = NULL;
			char *UUID = JSON_GetStr(row, "UUID");
			asprintf(&EXESpc, "%d bytes", GetUsedSpaceBytes(UUID, 1));

			//Write to dialog
			SetWindowText(GetDlgItem(hwnd, IDC_MAINEXESPACE), EXESpc);
			safe_free(EXESpc);
		} else {
			SetWindowText(GetDlgItem(hwnd, IDC_MAINEXESPACE), "");
		}
		safe_free(name);
		json_decref(out);
	} else {
		SetWindowText(GetDlgItem(hwnd, IDC_MAINNAME), "");
		SetWindowText(GetDlgItem(hwnd, IDC_MAINAUTHOR), "");
		SetWindowText(GetDlgItem(hwnd, IDC_MAINTYPE), "");
		SetWindowText(GetDlgItem(hwnd, IDC_MAINDESC), "");
		SetWindowText(GetDlgItem(hwnd, IDC_MAINVERSION), "");
		SetWindowText(GetDlgItem(hwnd, IDC_MAINEXESPACE), "");
	}

	return TRUE;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  GetUsedSpaceBytes
 *  Description:  Returns number of bytes used by a mod in the given file.
 * =====================================================================================
 */
int GetUsedSpaceBytes(const char *ModUUID, int File)
{
	int add, clear;
	sqlite3_stmt *command = NULL;
	const char query[] = "SELECT SUM(Len) FROM Spaces WHERE "
	                      "File = ? AND Mod = ? AND Type = ?;";

	if(SQL_HandleErrors(
		sqlite3_prepare_v2(
			CURRDB, query, strlen(query)+1,
			&command, NULL
		)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_int(command, 1, File) |
		sqlite3_bind_text(command, 2, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 3, "Add", -1, SQLITE_STATIC)
	) != 0){return -1;}

	add = SQL_GetNum(command);
	if(CURRERROR != errNOERR){return -1;}

	if(SQL_HandleErrors(sqlite3_reset(command)) != 0){
		CURRERROR = errCRIT_DBASE; return -1;
	}

	if(SQL_HandleErrors(
		sqlite3_bind_text(command, 3, "Clear", -1, SQLITE_STATIC)
	   ) != 0){return -1;}

	clear = SQL_GetNum(command);
	if(CURRERROR != errNOERR){return -1;}

	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return -1;
	}
	command = NULL;
	return (add - clear);
}

//Drop all non-numeric characters (except first decimal marker)
const char * ForceStrNumeric(const char *input)
{
	unsigned int i;
	BOOL decimalFound = FALSE;
	char *result = malloc(strlen(input)+1);
	char *resultAddr = result;

	for(i = 0; i < strlen(input); i++){
		if(isdigit(input[i]) || input[i] == ','){
			//Current char is a digit or thousands separator
			//(assuming ',')
			*resultAddr = input[i];
			resultAddr++;

		} else if (input[i] == '.' && !decimalFound){
			//Current chat is the first decimal separator
			//(assuming '.')
			*resultAddr = input[i];
			resultAddr++;
			decimalFound = TRUE;
		}
	}

	return result;
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

	//Look up https://blogs.msdn.microsoft.com/oldnewthing/20040621-00/?p=38793/
	//for details on what exactly I'm doing. It's weird and hacky.
	//(Also, I know thenewoldthing isn't official documentation, but
	//it's the best I've got. Sorry Raymond Chen.)

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
void DialogBoxSysFont(int DlgIDD, DLGPROC DlgProc, HWND hParent)
{
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
	DialogBoxIndirect(CURRINSTANCE, mainTemplate, hParent, DlgProc);

	safe_free(strIDD);
	safe_free(mainTemplate);
	return;
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

///SQLite helper Functions

//I'm sorry, I'm not typing all this out every time.
/*
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_ColName
 *  Description:  Returns the name of a column given it's index number
 * =====================================================================================
 */
const char * SQL_ColName(sqlite3_stmt * stmt, int col){
	return sqlite3_column_name(stmt, col) == NULL ?
		"" : (char *)sqlite3_column_name(stmt, col);
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_ColText
 *  Description:  Returns the value of an item given it's column index number
 * =====================================================================================
 */
const char * SQL_ColText(sqlite3_stmt * stmt, int col){
	return sqlite3_column_text(stmt, col) == NULL ?
		"" : (char *)sqlite3_column_text(stmt, col);
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_GetJSON
 *  Description:  Converts the result of an SQL statement to a easily searchable
 *                or savable JSON array.
 * =====================================================================================
 */
json_t * SQL_GetJSON(sqlite3_stmt *stmt)
{
	int i, errorNo;
	json_t *result = json_array();
	CURRERROR = errNOERR;

	///Compose the JSON array
	errorNo = sqlite3_step(stmt);
        while (errorNo == SQLITE_ROW){
		//For each row of results, create a new object
		json_t *tempobj = json_object();

		for(i = sqlite3_column_count(stmt) - 1; i >= 0; i--){
			//For each result key:value pair, insert into tempobj
			//key "colname" with value "coltext".
			json_t *value;
			char *colname = strdup(SQL_ColName(stmt, i));
			char *coltext = strdup(SQL_ColText(stmt, i));

			if(!colname || !coltext){
				CURRERROR = errCRIT_FUNCT;
				AlertMsg("SQL->JSON ERROR!", coltext);
				safe_free(colname);
				safe_free(coltext);
				return json_array();
			}


			if(strcmp(coltext, "") == 0){
				value = json_null();
			} else {
				value = json_string(coltext);
			}

			if(value == NULL){
				CURRERROR = errCRIT_FUNCT;
				AlertMsg("There was an error fetching data from"
					"the mod database. The application will"
					"now quit to prevent data corruption",
					"SQL->JSON ERROR!");
				safe_free(colname);
				safe_free(coltext);
				return json_array();
			}

			json_object_set_new(tempobj, colname, value);

			safe_free(colname);
			safe_free(coltext);
		}
		//Add object to array
		json_array_append_new(result, tempobj);

		errorNo = sqlite3_step(stmt);
        };

	if(errorNo != SQLITE_DONE){
		CURRERROR = errCRIT_DBASE;
		result = json_array();
	}

	//End array
	sqlite3_reset(stmt);

	return result;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_GetNum
 *  Description:  Returns the first item from an SQL query as a number
 *                (ex: the value of a COUNT statement)
 * =====================================================================================
 */
int SQL_GetNum(sqlite3_stmt *stmt)
{
	int errorNo, result = -1;
	CURRERROR = errNOERR;

        errorNo = sqlite3_step(stmt);
        if (errorNo == SQLITE_ROW) {
		result = sqlite3_column_int(stmt, 0);
        } else if (errorNo == SQLITE_DONE){
		result = -1;
	} else {
		CURRERROR = errCRIT_DBASE;
		result = -1;
	}

	sqlite3_reset(stmt);
	return result;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_GetStr
 *  Description:  Returns the first item from an SQL query as a string
 *                (ex: the value of a SELECT statement)
 * =====================================================================================
 */
char * SQL_GetStr(sqlite3_stmt *stmt)
{
	int errorNo;
	char *result = NULL;
	CURRERROR = errNOERR;

        errorNo = sqlite3_step(stmt);
        if (errorNo == SQLITE_ROW) {
		result = strdup(SQL_ColText(stmt, 0));
        } else if (errorNo == SQLITE_DONE){
		result = strdup("");
	} else {
		CURRERROR = errCRIT_DBASE;
	}

	sqlite3_reset(stmt);
	return result;
}

unsigned char * SQL_GetBlob(sqlite3_stmt *stmt, int *len)
{
	int errorNo;
	unsigned char *result = NULL;
	CURRERROR = errNOERR;

        errorNo = sqlite3_step(stmt);
        if (errorNo == SQLITE_ROW) {
		*len = sqlite3_column_bytes(stmt, 0);
		result = calloc(*len, sizeof(unsigned char));
		memcpy(result, sqlite3_column_blob(stmt, 0), *len);
        } else if (errorNo == SQLITE_DONE){
		result = strdup("");
	} else {
		CURRERROR = errCRIT_DBASE;
	}

	sqlite3_reset(stmt);
	return result;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_HandleErrors
 *  Description:  In the event of an error, handle and attempt to recover from it.
 * =====================================================================================
 */
int SQL_HandleErrors(int SQLResult)
{
	char *message = NULL;

	if(
		SQLResult == SQLITE_OK ||
		SQLResult == SQLITE_DONE ||
		SQLResult == SQLITE_ROW
	){
		return 0;
	}

	asprintf(&message, "Internal Database Error!\n"
		"Error code %d", SQLResult);
	AlertMsg(message, "SQLite Error!");
	safe_free(message);
	return -1;
}

///Jansson helper functions

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_Load
 *  Description:  Safely gives a json_t pointer to a JSON file
 *                given a filename. Only works with object roots.
 * =====================================================================================
 */

json_t * JSON_Load(const char *fpath)
{
	/// Load the file
	// Returns NULL on failure and root element on success
	json_error_t error;
	json_t *root = json_load_file(fpath, 0, &error);
	if(!root){
		char *buf;
		asprintf (&buf,
			"An error has occurred loading mod metadata.\n\n"
			"Contact the mod developer with the following info:\n"
			"line %d: %s", error.line, error.text);
		AlertMsg (buf, "Invalid JSON");
		safe_free(buf);
		return NULL;
	}
	if(!json_is_object(root)){
		AlertMsg ("An error has occurred loading mod metadata.\n\n"
			"Contact the mod developer with the following info:\n"
			"The root element of the JSON is not an object.",
			"Invalid JSON");
		return NULL;
	}
	return root;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_GetInt
 *  Description:  Gives a integer given a JSON tree and the name of a tag
 * =====================================================================================
 */
signed long JSON_GetInt(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);

	if (json_is_string(object)){
		char *temp = JSON_GetStr(root, name);
		signed long result = atol(temp);
		safe_free(temp);
		return result;
	} else if (json_is_number(object)){
		return json_integer_value(object);
	} else {
		return 0;
	}
}
/*
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_GetInt
 *  Description:  Gives a integer given a JSON tree and the name of a tag
 * =====================================================================================
 */
double JSON_GetDouble(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL) {return 0;}
	if (json_is_string(object)) {
		char *temp = JSON_GetStr(root, name);
		double result = atol(temp);
		safe_free(temp);
		return result;
	}
	return json_number_value(object);
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_GetStr
 *  Description:  Gives a string given a JSON tree and the name of a tag
 * =====================================================================================
 */
char * JSON_GetStr(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL){
		return strdup("");
	}
	if (json_is_integer(object)){
		int outint = JSON_GetInt(root, name);
		char *out = NULL;
		asprintf(&out, "%d", outint);
		return out;
	} else if (json_is_real(object)){
		double outint = JSON_GetDouble(root, name);
		char *out = NULL;
		asprintf(&out, "%f", outint);
		return out;
	}
	if (!json_is_string(object)){
		return strdup("");
	}
	return strdup(json_string_value(object));
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_GetStrLen
 *  Description:  Gives the length of a string given a JSON tree and the name of a tag
 * =====================================================================================
 */
int JSON_GetStrLen(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL) {
		return 0;
	}
	if (json_is_integer(object)) {
		return ItoaLen(JSON_GetInt(root, name));
	} else if (json_is_real(object)) {
		return FtoaLen(JSON_GetDouble(root, name));
	}
	if (!json_is_string(object)){
		return 0;
	}
	return json_string_length(object);
}

///Generic I/O Helper Functions

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Exists
 *  Description:  Determine if a file is present in the current directory
 *                and warns the user if it is not. Also optionally warns
 *                if file is read-only.
 * =====================================================================================
 */
BOOL File_Exists(const char file[MAX_PATH], BOOL InFolder, BOOL ReadOnly)
{
	int mode = ReadOnly ? (F_OK) : (F_OK || W_OK);
	CURRERROR = errNOERR;
	if (access(file, mode) != 0){
		switch(errno){
		case ENOENT: //File/Folder does not exist
			if(InFolder){CURRERROR = errWNG_BADDIR;}
			else{CURRERROR = errWNG_BADFILE;}
			return FALSE;
		case EACCES: // Read only error
			CURRERROR = errWNG_READONLY;
			return FALSE;
		}
	}
	return TRUE;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Known
 *  Description:  Tests if a file is found in the given whitelist by testing it's
 *                filesize and checksum. Alerts user accordingly if it isn't.
 * =====================================================================================
 */
BOOL File_Known(const char *FileName, json_t *whitelist)
{
	unsigned long fchecksum;
	signed long fsize;
	size_t i;
	json_t *value;
	CURRERROR = errNOERR;

	//Get filesize
	fsize = filesize(FileName);
	if(fsize == -1){
		CURRERROR = errWNG_BADFILE;
		return FALSE;
	}

	//Get the checksum of the file
	fchecksum = crc32File(FileName);
	if(fchecksum == 0){
		CURRERROR = errWNG_BADFILE;
		return FALSE;
	}

	//Report size and checksum
	/*{
		char *message = NULL;
		asprintf(&message,
			"%s\n CRC: %lX\n Size: %li",
			FileName, fchecksum, fsize);
		AlertMsg(message, "File Results");
		safe_free(message);
	}*/

	//Lookup file size and checksum in whitelist
	json_array_foreach(whitelist, i, value){
		char *temp = JSON_GetStr(value, "ChkSum");
		unsigned long ChkSum = strtoul(temp, NULL, 16);
		signed long Size = JSON_GetInt(value, "Size");
		safe_free(temp);

		if (fsize == Size && fchecksum == ChkSum){return TRUE;}
	}

	CURRERROR = errWNG_BADFILE;
	return FALSE;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  File_OpenSafe
 *  Description:  Safely opens a file by warning the user if the operation fails.
 *                Simple wrapper around io.h's _open function.
 * =====================================================================================
 */
int File_OpenSafe(const char *filename, int flags)
{
	int handle;
	int accessflag;
	//Check permissions first
	if((flags & _O_RDWR) || (flags & _O_WRONLY)){
		//Set flag to write access
		accessflag = W_OK;
	} else {
		accessflag = R_OK;
	}
	if(access(filename, accessflag) != 0){
		ErrNo2ErrCode();
		return -1;
	}

	handle = _open(filename, flags);
	//Make sure file is indeed open. (No reason it shouldn't be.)
	if (handle == -1){
		CURRERROR = errCRIT_FILESYS;
	}
	return handle;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Copy
 *  Description:  Copies a file from [OldPath] to [NewPath].
 *                Simple wrapper around Windows's CopyFile function on Win32.
 *                Might be more complicated on POSIX systems.
 * =====================================================================================
 */
void File_Copy(const char *OldPath, const char *NewPath)
{
	CopyFile(OldPath, NewPath, FALSE);
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Deltree
 *  Description:  Deletes a directory and all files and folders within.
 *                If using POSIX libraries, it uses the function File_Deltree_CB()
 *                as a callback function for ntfw().
 * =====================================================================================
 */
//#ifdef ntfw
//POSIX routine
/*int File_Deltree_CB(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	return remove(fpath);
}

void File_Deltree(char *DirPath)
{
	nftw(DirPath, File_Deltree_CB, 64, FTW_DEPTH | FTW_PHYS);
}
*/
//#else
//Generic routine
/*void File_Deltree(char *DirPath)
{
	DIR *DirID;
	struct dirent *DirEnt;

	DirID = opendir (DirPath);
	if (DirID == NULL){return;}

	while ((DirEnt = readdir(DirID)) != NULL){
		struct stat *FileEnt;
		stat(DirEnt->d_name, FileEnt);

		if(S_ISDIR(FileEnt->st_mode)){
			char *NewPath = NULL;
			asprintf(NewPath, "%s\\%s", DirPath, DirEnt->d_name);
			File_Deltree(NewPath);
		}

		remove(DirEnt->d_name);
	}

	closedir(DirID);
	return;
}*/
//#endif

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Movtree (WIP)
 *  Description:  Moves a directory and all files and folders within.
 * =====================================================================================
 */
/*void File_Movtree(char *SrcPath, char *DestPath)
{
	DIR *DirID;
	struct dirent *DirEnt;

	DirID = opendir (SrcPath);
	if (DirID == NULL){return;}

	while ((DirEnt = readdir(DirID)) != NULL){
		struct stat *FileEnt;
		stat(DirEnt->d_name, FileEnt);

		if(S_ISDIR(FileEnt->st_mode)){
			//Recursion!
			File_Movtree(DirEnt->d_name);
		}

		{

			rename(DirEnt->d_name, DestPath);
		}
	}

	closedir(DirID);
	return;
}*/

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  File_TransactWrite
 *  Description:  Marks [Bytes] as to be inserted into [FilePath] when
 *                File_TransactCommit() is called.
 *                Implemented by creating temp files and moving over on Win32.
 * =====================================================================================
 */
void File_TransactWrite()
{
	//Check if .\TEMP exists
		//If not, create .\TEMP
	//Check if .\TEMP\[filename] exists
		//If not, copy [filename] to .\TEMP.

	//Write [Bytes] to .\TEMP\[filename]
}

void File_TransactCommit()
{
	//Move .\TEMP\* to .\, merging in process

	//Delete .\TEMP
}

void File_TransactRollback()
{
	//Delete .\TEMP and all files
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  File_WriteBytes
 *  Description:  Given an offset and a byte arry of data write it to the given file.
 * =====================================================================================
 */
void File_WriteBytes(int filehandle, int offset, unsigned const char *data, int datalen)
{
	lseek(filehandle, offset, SEEK_SET);
	write(filehandle, data, datalen);
	return;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  ItoaLen
 *  Description:  Returns length in bytes the given number would take up if
 *                converted to a string via, ex, itoa().
 * =====================================================================================
 */
int ItoaLen(int input){
	return snprintf(NULL, 0, "%d", input);
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  FtoaLen
 *  Description:  Returns length in bytes the given floating point number would
 *                take up if converted to a string via, ex, ftoa().
 * =====================================================================================
 */
int FtoaLen(double input){
	return snprintf(NULL, 0, "%f", input);
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  MultiMax
 *  Description:  Returns the maximum value of a list of floating point arguments
 * =====================================================================================
 */
double MultiMax(double count, ...)
{
	va_list args;
	double result = -HUGE_VAL;

	va_start(args, count);
	for(; count == 0; count--){
		double temp = va_arg(args, double);
		if(temp > result){temp = result;}
	}
	va_end(args);
	return result;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  Hex2Bytes
 *  Description:  Converts a hexadecimal string to an array of bytes.
 * =====================================================================================
 */

unsigned char * Hex2Bytes(const char *hexstring, int *len)
{
	int i = 0;
	//Allocate space for every byte + null byte (just in case)
	unsigned char *result = malloc((strlen(hexstring) / 2) + 1);
	char currByte[3];
	const char *currBytePtr = hexstring;

	while(*currBytePtr != '\0'){
		//Only because we need the null terminator
		strncpy(currByte, currBytePtr, 2);
		currByte[2] = '\0';
		result[i] = (char)strtoul(currByte, NULL, 16);

		currBytePtr += 2;
		i++;
	}

	if(len != NULL){*len = i;}
	return result;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  Bytes2Hex
 *  Description:  Converts an array of bytes to a hexadecimal string.
 * =====================================================================================
 */
char * Bytes2Hex(unsigned const char *bytes, int len)
{
	char *val;
	int i = 0;

	val = calloc(len*4, sizeof(char));
	for(i = 0; i <= len; i++) {
		char *temp;
		asprintf(&temp, "%02X", bytes[i]);
		strcat(val, temp);
		free(temp);
	}
	return val;
}

///Win32-specific IO functions

#ifndef filesize
/*
 * ===  FUNCTION  ======================================================================
 *         Name:  filesize
 *  Description:  Drop-in replacement for filesize in io.h
 * =====================================================================================
 */
long filesize(const char *filename){
	unsigned long len;
	HANDLE input = INVALID_HANDLE_VALUE;

	/* Open file */
	input = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(input == INVALID_HANDLE_VALUE){return -1;}

	/* Get file size */
	len = GetFileSize(input, NULL);

	/* Close handle */
	CloseHandle(input);

	/* Return value */
	if(len == INVALID_FILE_SIZE){
		return -1;
	} else {
		return (long)len;
	}
}
#endif

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  SelectInstallFolder
 *  Description:  Picks a directory and tests if the contained files are valid according
 *                to a given configuration file.
 * =====================================================================================
 */
char * SelectInstallFolder(json_t *GameCfg)
{
	char *path = NULL;
	char *title = NULL;
	char *GameName = NULL;
	char *GameEXE = NULL;
	json_t *whitelist = json_object_get(GameCfg, "Whitelist");
	CURRERROR = errNOERR;

	//Verify existence of whitelist object
	if(!whitelist || !json_is_array(whitelist)){
		CURRERROR = errWNG_CONFIG;
		return "";
	}

	GameName = JSON_GetStr(GameCfg, "GameName");
	GameEXE = JSON_GetStr(GameCfg, "GameEXE");
	asprintf(&title, "Pick %s Installation directory.", GameName);
	safe_free(GameName);

	//Pick directory
	path = SelectFolder(title);
	safe_free(title);

	//Make sure path isn't null (it probably isn't, but...)
	if(path == NULL){
		//I'm not even sure when that would happen.
		safe_free(GameEXE);
		return "";
	}

	//If there's an error, just pass it through
	if (CURRERROR != errNOERR) {
		safe_free(path);
		safe_free(GameEXE);
		return "";
	}

	//Set working directory to selected directory
	chdir(path);

	// Make sure it has the game file
	if(!File_Exists(GameEXE, TRUE, FALSE)){
		safe_free(GameEXE);
		safe_free(path);
		return "";
	}

	//Check if file is on whitelist
	if(File_Known(GameEXE, whitelist) == FALSE){
		safe_free(GameEXE);
		safe_free(path);
		return "";
	}

	//The path is good!
	safe_free(GameEXE);
	CURRERROR = errNOERR;
	return path;
}

/// Mod Loading Routines
//////////////////////////////////
/*
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_CheckCompat
 *  Description:  Checks to make sure that the mod selected is in fact compatible
 *                with the currently installed mods and the mod loader.
 * =====================================================================================
 */
BOOL Mod_CheckCompat(json_t *root)
{
	// Check mod loader version
	int error_no, ML_Maj = 0, ML_Min = 0, ML_Bfx = 0, ML_Ver = 0;
	char *ML_Str;

	// Make sure there even is a version string
	if (
		!root ||
		!json_object_get(root, "ML_Ver") ||
		!json_is_string(json_object_get(root, "ML_Ver"))
	){
		AlertMsg ("An error has occurred loading mod metadata.\n\n"
			"Contact the mod developer with the following info:\n"
			"No mod loader version string was found.\n"
			"It should be named 'ML_Ver' and be in the format\n"
			"[MAJOR].[MINOR].[BUGFIX]",
			"Invalid JSON");
		return FALSE;
	}

	ML_Str = JSON_GetStr(root, "ML_Ver");
	error_no = sscanf(ML_Str, "%d.%d.%d", &ML_Maj, &ML_Min, &ML_Bfx);
	safe_free(ML_Str);

	//Make sure version string is formatted properly
	if (error_no != 3){
		AlertMsg ("This mod is trying to ask for an invalid mod loader version.\n\n"
			"Please contact the mod developer with the following information:\n"
			"ML_Ver string is not in format [MAJOR].[MINOR].[BUGFIX]",
			"Invalid Version Requested");
		return FALSE;
	}

	//Compare versions.
	ML_Ver = ML_Bfx + (ML_Min*0x100) + (ML_Maj*0x10000);
	if(ML_Ver > PROGVERSION){
		char *buf = NULL;
		asprintf (&buf,
			"This mod requires a newer version of the mod loader.\n"
			"You have version %d.%d.%d and need version %d.%d.%d.\n"
			"Obtain an updated copy at\n%s",
			PROGVERSION_MAJOR, PROGVERSION_MINOR, PROGVERSION_BUGFIX,
			ML_Maj, ML_Min, ML_Bfx, PROGSITE);
		AlertMsg (buf, "Outdated Mod Loader");
		safe_free(buf);
		return FALSE;
	}
	return TRUE;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_CheckConflict
 *  Description:  Checks if a mod with the current name is already installed
 * =====================================================================================
 */
enum Mod_CheckConflict_RetVals Mod_CheckConflict(json_t *root)
{
	int oldver;
	BOOL FirstInstall = FALSE;

	// Check for conflicts
	{
		sqlite3_stmt *command;
		const char *query = "SELECT Ver FROM Mods WHERE UUID = ?;";
		char *UUID = JSON_GetStr(root, "UUID");
		CURRERROR = errNOERR;

		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT)
		) != 0){safe_free(UUID);
			CURRERROR = errCRIT_DBASE; return ERR;}

		oldver = SQL_GetNum(command);
		//Check that GetNum succeeded
		if(CURRERROR != errNOERR){return ERROR;}
		if(oldver == -1){FirstInstall = TRUE;}

		if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		safe_free(UUID);
	}
	if (oldver != 0){
		char *name = JSON_GetStr(root, "Name");
		int MBVal = IDYES, ver; // Message box response, version req.

		//Check version number
		ver = JSON_GetInt(root, "Ver");
		//Automatically install if not installed at all
		if(FirstInstall == TRUE){oldver = ver;}

		if(ver > oldver){
			char *buf = NULL;
			asprintf (&buf,
				"The mod you have selected is a newer version of\n"
				"'%s', which you have already installed.\n"
				"Do you want to upgrade from version %d to version %d?",
				name, oldver, ver);
			if(buf == NULL){
				AlertMsg("Malloc Fail #4", "");
				safe_free(name);
				CURRERROR = errCRIT_MALLOC;
				return ERR;
			}
			MBVal = MessageBox (0, buf, "Mod Upgrade", MB_ICONEXCLAMATION | MB_YESNO);
			safe_free(buf);
		}
		else if (ver < oldver){
			char *buf = NULL;
			asprintf (&buf,
				"The mod you have selected is an OLDER version of\n"
				"'%s', which you have already installed.\n"
				"Do you want to downgrade from version %d to version %d?",
				name, oldver, ver);
			if(buf == NULL){
				AlertMsg("Malloc Fail #5", "");
				safe_free(name);
				CURRERROR = errCRIT_MALLOC;
				return ERR;
			}
			MBVal = MessageBox (0, buf, "Mod Downgrade", MB_ICONEXCLAMATION | MB_YESNO);
			safe_free(buf);
		}
		else if (ver == oldver && FirstInstall == FALSE){
			char *buf = NULL;
			asprintf (&buf, "%s is already installed.", name);
			if(buf == NULL){
				AlertMsg("Malloc Fail #6", "");
				safe_free(name);
				CURRERROR = errCRIT_MALLOC;
				return ERR;
			}
			AlertMsg (buf, "Mod Already Installed");
			MBVal = IDNO;
			safe_free(buf);
		}

		safe_free(name);
		if (MBVal == IDNO){return CANCEL;} //Do not replace
		else{return GO;} //Do replace
	}

	return GO; //No conflict
}

//False == ERROR or Dependency issue
//True == No dependency issues (or dependencies, period)!
//TODO: Less confusing name? (This checks for presence of requested mods)
BOOL Mod_CheckDep(json_t *root)
{
	unsigned int i;
	json_t *value, *deps;
	char *ParentUUID = JSON_GetStr(root, "UUID");
	CURRERROR = errNOERR;

	if(ParentUUID == NULL){return FALSE;}

	deps = json_object_get(root, "dependencies");
	if(!json_is_array(deps)){
		//No dependencies. Therefore, no issues!
		safe_free(ParentUUID);
		return TRUE;
	}

	json_array_foreach(deps, i, value){
		int ModCount;
		{
			sqlite3_stmt *command;
			const char *query = "SELECT COUNT(*) FROM Mods WHERE UUID = ? AND Ver >= ?";
			char *UUID = JSON_GetStr(value, "UUID");
			int ver = JSON_GetInt(value, "Ver");

			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT) |
				sqlite3_bind_int(command, 2, ver)
			) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}

			ModCount = SQL_GetNum(command);
			if(CURRERROR != errNOERR){return FALSE;}

			if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
				CURRERROR = errCRIT_DBASE; return FALSE;
			}
			safe_free(UUID);
		}

		if (ModCount == 0){
			//There are missing mods. Roll back and abort.
			sqlite3_stmt *command;
			const char *query = "DELETE FROM Dependencies WHERE ParentUUID = ?;";
			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_text(command, 1, ParentUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(sqlite3_step(command)
			) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
			) != 0){
				CURRERROR = errCRIT_DBASE; return FALSE;
			}
			command = NULL;
			safe_free(ParentUUID);
			return FALSE;
		}

		else{
			//Insert that dependency into the table for uninstallation purposes
			sqlite3_stmt *command;
			const char *query = "INSERT INTO Dependencies "
				"('ChildUUID', 'ParentUUID')  VALUES (?, ?);";
			char *UUID = JSON_GetStr(value, "UUID");
			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT) |
				sqlite3_bind_text(command, 2, ParentUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(sqlite3_step(command)
			) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
			) != 0){
				CURRERROR = errCRIT_DBASE; return FALSE;
			}
			command = NULL;
			safe_free(UUID);
		}
	}
	safe_free(ParentUUID);
	return TRUE;
}

//Alert helper function
void Mod_CheckDepAlert(json_t *root){
	char *message = NULL;
	unsigned int messagelen;
	unsigned int i;
	json_t *value, *deps;
	CURRERROR = errNOERR;

	{
		char *parName = JSON_GetStr(root, "Name");
		char *parAuth = JSON_GetStr(root, "Auth");
		int parVer = JSON_GetInt(root, "Ver");
		messagelen = asprintf(&message,
			"The mod you are trying to install,\n"
			"%s version %d by %s, requires some mods\n"
			"that you do not have installed:\n\n",
			parName, parVer, parAuth
		);
		safe_free(parName);
		safe_free(parAuth);
	}

	deps = json_object_get(root, "dependencies");
	if (!json_is_array(deps)){
		//No dependencies.
		safe_free(message);
		return;
	}

	json_array_foreach(deps, i, value){
		int ModCount;

		{
			sqlite3_stmt *command;
			const char *query = "SELECT COUNT(*) FROM Mods "
			                    "WHERE UUID = ? AND Ver >= ?";
			char *UUID = JSON_GetStr(value, "UUID");
			int ver = JSON_GetInt(value, "Ver");

			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT) |
				sqlite3_bind_int(command, 2, ver)
			) != 0){
				CURRERROR = errCRIT_DBASE;
				safe_free(UUID);
				safe_free(message);
				return;
			}

			ModCount = SQL_GetNum(command);
			if(CURRERROR != errNOERR){return;}

			if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
				CURRERROR = errCRIT_DBASE;
				safe_free(UUID);
				safe_free(message);
				return;
			}
			safe_free(UUID);
		}

		if (ModCount == 0){ //If mod not installed
			char *name = JSON_GetStr(value, "Name");
			char *auth = JSON_GetStr(value, "Auth");
			int ver = JSON_GetInt(value, "Ver");
			char *out = NULL;

			asprintf(&out, "- %s version %d by %s\n", name, ver, auth);
			if(out == NULL){
				AlertMsg("Malloc Fail #7", "");
				CURRERROR = errCRIT_MALLOC;
				safe_free(name);
				safe_free(auth);
				return;
			}

			while(strlen(message) + strlen(out) + 1 >= messagelen){
				messagelen *= messagelen;
				message = realloc(message, messagelen);
				if(message == NULL){
					AlertMsg("Malloc Fail #8", "");
					CURRERROR = errCRIT_MALLOC;
					safe_free(name);
					safe_free(auth);
					safe_free(out);
					return;
				}
			}
			strcat(message, out);

			safe_free(name);
			safe_free(auth);
			safe_free(out);
		}
	}

	AlertMsg(message, "Other Mods Required");
	safe_free(message);
}

///Mod Installation Routines
////////////////////////////////

int File_GetID(const char * FileName)
{
	sqlite3_stmt *command;
	int ID = -1;
	const char *query1 = "SELECT ID FROM Files WHERE Path = ?;";

	CURRERROR = errNOERR;

	//Get the ID
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_text(command, 1, FileName, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return -1;}

	ID = SQL_GetNum(command);

	if(CURRERROR != errNOERR){return -1;}
	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return -1;
	}

	if(ID == -1){
		//Add new entry into DB
		ID = File_MakeEntry(FileName);
		//ID might be -1. Return that and let host deal with it.
	}

	return ID;
}

char * File_GetName(int input)
{
	sqlite3_stmt *command;
	const char *query = "SELECT Path FROM Files WHERE ID = ?;";
	char *output = NULL;
	CURRERROR = errNOERR;

	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_int(command, 1, input)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return "";
	}

	output = SQL_GetStr(command);

	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		safe_free(output);
		return "";
	}
	command = NULL;
	return output;
}

int File_MakeEntry(const char *FileName){
	sqlite3_stmt *command;
	const char *query1 = "SELECT MAX(ID) FROM Files;";
	const char *query2 = "INSERT INTO Files (ID, Path) VALUES (?, ?);";
	int IDCount, ID;
	CURRERROR = errNOERR;

	//Get highest ID assigned
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0){CURRERROR = errCRIT_DBASE; return -1;}

	IDCount = SQL_GetNum(command);

	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return -1;
	}
	//Check that GetNum succeeded
	if(IDCount == -1 || CURRERROR != errNOERR){return -1;}

	//New ID is highest ID + 1
	ID = IDCount + 1;

	//Insert new ID into database
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_int(command, 1, ID) |
		sqlite3_bind_text(command, 2, FileName, -1, SQLITE_TRANSIENT)
	) != 0 || SQL_HandleErrors(sqlite3_step(command)
	) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return -1;
	}
	command = NULL;

	//Return new ID in case function needs it
	return ID;
}

//Effectively this function is a poor-man's string lookup table.
enum VarType Var_GetType(const char *type)
{
	enum VarType result;
	if(strcmp(type, "IEEE32") == 0){
		result = IEEE32;
	} else if (strcmp(type, "IEEE64") == 0){
		result = IEEE64;
	} else if (strcmp(type, "Int8") == 0){
		result = Int8;
	} else if (strcmp(type, "Int16") == 0){
		result = Int16;
	} else if(strcmp(type, "Int32") == 0){
		result = Int32;
	} else if (strcmp(type, "uInt8") == 0){
		result = uInt8;
	} else if (strcmp(type, "uInt16") == 0){
		result = uInt16;
	} else if (strcmp(type, "uInt32") == 0){
		result = uInt32;
	} else {
		result = INVALID;
	}
	return result;
}

//Ditto, except this takes a VarType enum and spits out a textual representation.
const char * Var_GetType_Str(enum VarType type){
	switch(type){
	case IEEE64: return "IEEE64";
	case IEEE32: return "IEEE32";
	case Int8:   return "Int8";
	case Int16:  return "Int16";
	case Int32:  return "Int32";
	case uInt8:  return "uInt8";
	case uInt16: return "uInt16";
	case uInt32: return "uInt32";
	default:     return "INVALID";
	}
}

//Fetches bytes from stored SQL table
struct VarValue Var_GetValue_SQL(const char *VarUUID){
	struct VarValue result;
	json_t *VarArr, *VarObj;
	sqlite3_stmt *command;
	char *query = "SELECT * FROM Variables WHERE UUID = ?";
	CURRERROR = errNOERR;

	//Get SQL results and put into VarObj
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_text(command, 1, VarUUID, -1, SQLITE_TRANSIENT)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		result.type = INVALID;
		return result;
	}

	VarArr = SQL_GetJSON(command);

	if(CURRERROR != errNOERR){
		CURRERROR = errCRIT_DBASE;
		result.type = INVALID;
		return result;
	}
	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		result.type = INVALID;
		return result;
	}

	VarObj = json_array_get(VarArr, 0);

	//Get type
	result.type = Var_GetType(JSON_GetStr(VarObj, "Type"));

	//Get value
	switch(result.type){
	case IEEE64:
		result.IEEE64 = (double)JSON_GetDouble(VarObj, "Value"); break;
	case IEEE32:
		result.IEEE32 = (float)JSON_GetDouble(VarObj, "Value"); break;
	case Int32:
		result.Int32  = (int32_t)JSON_GetInt(VarObj, "Value"); break;
	case Int16:
		result.Int16  = (int16_t)JSON_GetInt(VarObj, "Value"); break;
	case Int8:
		result.Int8   = (int8_t)JSON_GetInt(VarObj, "Value"); break;
	case uInt32:
		result.uInt32 = (uint32_t)JSON_GetInt(VarObj, "Value"); break;
	case uInt16:
		result.uInt16 = (uint16_t)JSON_GetInt(VarObj, "Value"); break;
	case uInt8:
		result.uInt8  = (uint8_t)JSON_GetInt(VarObj, "Value"); break;
	}

	//Get UUID, desc, publicType
	result.UUID = JSON_GetStr(VarObj, "UUID");
	result.desc = JSON_GetStr(VarObj, "Desc");
	result.publicType = JSON_GetStr(VarObj, "PublicType");
	result.mod = JSON_GetStr(VarObj, "Mod");

	json_decref(VarObj);
	json_decref(VarArr);
	return result;
}

//Converts JSON to
struct VarValue Var_GetValue_JSON(json_t *VarObj){
	struct VarValue result;
	char *value = NULL;

	//Get type
	{
		char *type = JSON_GetStr(VarObj, "Type");
		result.type = Var_GetType(type);
		safe_free(type);
	}

	//Value insurance in case things go wrong
	switch(result.type){
	case IEEE64:
		result.IEEE64 = 0; break;
	case IEEE32:
		result.IEEE32 = 0; break;
	default: //If it's an integer, this will be straight 0s.
		result.uInt32 = 0; break;
	}

	value = JSON_GetStr(VarObj, "Value");

	//Check for + sign.
	if(value[0] == '+'){
		//Get stored value of variable
		result = Var_GetValue_SQL(JSON_GetStr(VarObj, "UUID"));
		memmove(value, value+1, strlen(value)+1); //Remove + from string
	}
	//This ends up doing a funky thing where to subtract a number you have
	//to type +-999, but how else would we do it without just adding another
	//key? In hindsight that would make sense, but still.

	//Get value
	switch(result.type){
	case IEEE64:
		result.IEEE64 += (double)atof(value);  break;
	case IEEE32:
		result.IEEE32 += (float)atof(value);  break;
	case Int32:
		result.Int32  += (int32_t)atoi(value);  break;
	case Int16:
		result.Int16  += (int16_t)atoi(value);  break;
	case Int8:
		result.Int8   += (int8_t)atoi(value);  break;
	case uInt32:
		result.uInt32 += (uint32_t)atoi(value);  break;
	case uInt16:
		result.uInt16 += (uint16_t)atoi(value);  break;
	case uInt8:
		result.uInt8  += (uint8_t)atoi(value);  break;
	}

	//Get UUID, desc, publicType
	result.UUID = JSON_GetStr(VarObj, "UUID");
	result.desc = JSON_GetStr(VarObj, "Desc");
	result.publicType = JSON_GetStr(VarObj, "PublicType");
	result.mod = JSON_GetStr(VarObj, "Mod");

	safe_free(value);
	return result;
}

//Create Public Variable Listbox entires in SQL database
void Var_CreatePubList(json_t *PubList, const char *UUID){
	json_t *row;
	unsigned int i;
	CURRERROR = errNOERR;

	json_array_foreach(PubList, i, row){
		char *label = JSON_GetStr(row, "Label");
		int num = JSON_GetInt(row, "Number");
		sqlite3_stmt *command;
		char *query = "INSERT INTO VarList (Var, Number, Label) "
		              "VALUES (?, ?, ?);";

		//Get SQL results and put into VarObj
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_int(command, 2, num) |
			sqlite3_bind_text(command, 3, label, -1, SQLITE_TRANSIENT)
		) != 0 || SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			safe_free(label);
			CURRERROR = errCRIT_DBASE;
			return;
		}
		command = NULL;
		safe_free(label);
	}
}

BOOL Var_ClearEntry(const char *ModUUID)
{
	//This is really dumb. Eh.
	sqlite3_stmt *command;
	const char *query = "DELETE FROM Variables WHERE Mod = ?";

	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
	) != 0 || SQL_HandleErrors(sqlite3_step(command)
	) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}

	return TRUE;
}

BOOL Var_MakeEntry(json_t *VarObj, const char *ModUUID)
{
	struct VarValue result;
	CURRERROR = errNOERR;
	//VarObj is read from mod's file.

	//Get variable value and type
	result = Var_GetValue_JSON(VarObj);
	AlertMsg(result.UUID, "Variable UUID");

	//Check if variable already exists
	if(Var_GetValue_SQL(result.UUID).type != INVALID){
		//If so, update existing variable instead of making a new one.

		sqlite3_stmt *command;
		const char *query = "UPDATE Variables SET "
		                    "'Value' = ? WHERE UUID = ?;";
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 2, result.UUID, -1, SQLITE_TRANSIENT)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}

		//Swap out Value for appropriate type.
		switch(result.type){
		case IEEE64:
			sqlite3_bind_double(command, 1, result.IEEE64); break;
		case IEEE32:
			sqlite3_bind_double(command, 1, result.IEEE32); break;
		case Int32:
			sqlite3_bind_int(command, 1, result.Int32); break;
		case Int16:
			sqlite3_bind_int(command, 1, result.Int16); break;
		case Int8:
			sqlite3_bind_int(command, 1, result.Int8); break;
		case uInt32:
			sqlite3_bind_int(command, 1, result.uInt32); break;
		case uInt16:
			sqlite3_bind_int(command, 1, result.uInt16); break;
		case uInt8:
			sqlite3_bind_int(command, 1, result.uInt8); break;
		default:
			return FALSE;
		}

		if(SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
		return TRUE;
	}

	//If PublicType == list, create and insert the PublicList
	if(strcmp(result.publicType, "list") == 0){
		json_t *PubList = json_object_get(VarObj, "PublicList");
		Var_CreatePubList(PubList, result.UUID);
	}


	//Compose and execute an insert command.
	{
		sqlite3_stmt *command;
		char *query = "INSERT INTO Variables "
		              "(UUID, Mod, Type, PublicType, Desc, Value) "
			      "VALUES (?, ?, ?, ?, ?, ?);";

		//Compose SQL
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, result.UUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 2, ModUUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 3, Var_GetType_Str(result.type), -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 4, result.publicType, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 5, result.desc, -1, SQLITE_TRANSIENT)
		   ) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}

		//Swap out Value for appropriate type.
		switch(result.type){
		case IEEE64:
			sqlite3_bind_double(command, 6, result.IEEE64); break;
		case IEEE32:
			sqlite3_bind_double(command, 6, result.IEEE32); break;
		case Int32:
			sqlite3_bind_int(command, 6, result.Int32); break;
		case Int16:
			sqlite3_bind_int(command, 6, result.Int16); break;
		case Int8:
			sqlite3_bind_int(command, 6, result.Int8); break;
		case uInt32:
			sqlite3_bind_int(command, 6, result.uInt32); break;
		case uInt16:
			sqlite3_bind_int(command, 6, result.uInt16); break;
		case uInt8:
			sqlite3_bind_int(command, 6, result.uInt8); break;
		default:
			sqlite3_bind_int(command, 6, 0); break;
		}
		if(SQL_HandleErrors(sqlite3_step(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
	}

	//Free junk in those variables
	safe_free(result.UUID);
	safe_free(result.desc);
	safe_free(result.publicType);
	safe_free(result.mod);

	return TRUE;
}

//Take Var from SQL and convert to bytes, I guess.
void Var_SetValue(json_t VarObject, int value)
{

}

//Variable Dialog Procedure
//WIN32 only!

BOOL CALLBACK Dlg_Var_Resize(HWND hCtl, LPARAM width)
{
	int wCtl = width / 3; //66%!
	int xCtl = width - (21 + wCtl);
	int wLbl = width - xCtl - 10;
	char ctlClass[64];
	RECT ctlRect;

	//Get child type
	GetClassName(hCtl, ctlClass, 64);
	//Get sizes
	GetWindowRect(hCtl, &ctlRect);
	MapWindowPoints(HWND_DESKTOP, GetParent(hCtl), (LPPOINT) &ctlRect, 2);

	//Check child type
	if(
		strcmp(ctlClass, "Button") == 0 || //Checkbox
		strcmp(ctlClass, "Edit") == 0 //Numeric
	){
		//Control!
		MoveWindow(
			hCtl,
			xCtl, ctlRect.top,
			wCtl, (ctlRect.bottom - ctlRect.top),
			TRUE
		);
	} else if(
		strcmp(ctlClass, "Static") == 0 // Label
	) {
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
			xCtl + wCtl - 2, ctlRect.top,
			(ctlRect.right - ctlRect.left),
			(ctlRect.bottom - ctlRect.top),
			TRUE
		);
	} else {
		//Probably an up-down or other supporter.
		//No need to mess with.
	}

	return TRUE;
}

/*BOOL CALLBACK Dlg_Var_Save(HWND hCtl, LPARAM lParam)
{
	//For each control:
	char *VarUUID = NULL;
	char *CtlType = NULL;
	struct VarValue CurrVar;

	//Make sure it *is* a control
	VarUUID = GetProp(hCtl, "VarUUID");
	if (VarUUID == NULL){return FALSE;}

	//Switch depending on control type
	CtlType = GetProp(hCtl, "CtlType");

	if (strcmp(CtlType, "Numeric") == 0){
		switch(HIWORD(wParam)){
		case EN_UPDATE:{
			//Get contents of buffer

			//Force contents to numeric
		}

		break;
		}
	} else if (strcmp(CtlType, "") == 0){ //?? (Really need my docs)

	}
}*/

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
		}

		ScrollPosNew = min(ScrollPosNew, TotalSize);
		ScrollPosNew = max(0, ScrollPosNew);

		{
			SCROLLINFO SbInfo;

			//Set scroll box position
			SbInfo.cbSize = sizeof(SCROLLINFO);
			SbInfo.fMask = SIF_POS;
			SbInfo.nPos = ScrollPosNew;
			SetScrollInfo(hwnd, SB_VERT, &SbInfo, TRUE);

			//Then get it, because Windows probably adjusted it
			GetScrollInfo(hwnd, SB_VERT, &SbInfo);
			ScrollPosNew = SbInfo.nPos;

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

			DestroyWindow(hwnd);
		break;
		}

		default:
			DestroyWindow(hwnd);
		break;

		}
	break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}

//Generate a window containing a mod's variables
//WIN32 only!
HWND Var_GenWindow(const char *ModUUID, HWND hDlgMain)
{
	//Define variables
	HWND BigWin = NULL;
	HWND VarWin = NULL;

	json_t *VarArray, *VarCurr_JSON;
	size_t i;
	RECT BigWinSize, VarWinSize, ConfWinSize;

	//Set window title
	char *winTitle = NULL;
	asprintf(&winTitle, "Options for %s", ModUUID);
	AlertMsg(winTitle, "Window title");

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
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			//Destroy window
			return NULL;
		}

		VarArray = SQL_GetJSON(command);

		if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
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
		//TODO: Make sure these numbers are sane. They probably aren't.
		int wCtl = VarWinSize.right / 3;
		int xCtl = VarWinSize.right - (21 + wCtl);
		int yCtl = (i * 27)+14;

		int wLbl = 200;
		int xLbl = 10;
		int hCtl = 20;

		struct VarValue VarCurr = Var_GetValue_JSON(VarCurr_JSON);

		//Map Y coordinates to proper text size
		RECT CtlCoord;
		CtlCoord.left = 0;
		CtlCoord.top = 20;
		CtlCoord.right = 0;
		CtlCoord.bottom = yCtl;
		MapDialogRect(hDlgMain, &BigWinSize);
		hCtl = CtlCoord.top;
		yCtl = CtlCoord.bottom;

		//Generate label

		//If I weren't lazy, I'd draw this with GDI.
		//Unfortunately, this is [CURRENTYEAR], so I will be.
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
			strcmp(VarCurr.publicType, "Numeric") == 0 ||
			strcmp(VarCurr.publicType, "HexNum") == 0
		){
			HWND hNumControl;
			HWND hSpinControl;

			//Create textbox
			hNumControl = CreateWindowEx(
				WS_EX_CLIENTEDGE,
				"EDIT", "0",
				WS_CHILD | WS_VISIBLE,
				xCtl, yCtl,
				wCtl, hCtl,
				VarWin,
				NULL,
				//(HMENU)(90000)+i, //Unique numeric identifier
				CURRINSTANCE, NULL
			);

			//Set UUID as window property
			SetProp(hNumControl, "VarUUID", strdup(VarCurr.UUID));
			//Set control type as window property
			SetProp(hNumControl, "CtlType", "Numeric");

			///Create up/down
			hSpinControl = CreateWindow(
				UPDOWN_CLASS, NULL,
				UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_WRAP | WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT,
				xCtl, yCtl,
				wCtl, hCtl,
				VarWin,
				NULL,
				//(HMENU)(91000)+i, //Unique numeric identifier
				CURRINSTANCE, NULL
			);

			//Set textbox to be buddy of up/down
			//to let textbox show contents of spinbox
			SendMessage(hSpinControl, UDM_SETBUDDY, (WPARAM)hNumControl, NULL);

			if(strcmp(VarCurr.publicType, "HexNum") == 0){
				//Hex view only supports unsigned values.
				//Set range/value to whatever it's saved as
				switch(VarCurr.type){
				default:
				case Int32:
				case uInt32:
					SendMessage(hSpinControl, UDM_SETRANGE32, 0, ULONG_MAX);
					SendMessage(hSpinControl, UDM_SETPOS32, 0, VarCurr.uInt32);
					break;
				case Int16:
				case uInt16:
					SendMessage(hSpinControl, UDM_SETRANGE32, 0, USHRT_MAX);
					SendMessage(hSpinControl, UDM_SETPOS32, 0, VarCurr.uInt16);
					break;
				case Int8:
				case uInt8:
					SendMessage(hSpinControl, UDM_SETRANGE32, 0, UCHAR_MAX);
					SendMessage(hSpinControl, UDM_SETPOS32, 0, VarCurr.uInt8);
					break;
				}
				//Set spinner mode to hex
				SendMessage(hSpinControl, UDM_SETBASE, 16, 0);
			} else {
				//Set range depending on type
				switch(VarCurr.type){
				default:
				case Int32:
					SendMessage(hSpinControl, UDM_SETRANGE32, LONG_MIN, LONG_MAX);
					SendMessage(hSpinControl, UDM_SETPOS32, 0, VarCurr.Int32);
					break;
				case Int16:
					SendMessage(hSpinControl, UDM_SETRANGE32, SHRT_MIN, SHRT_MAX);
					SendMessage(hSpinControl, UDM_SETPOS32, 0, VarCurr.Int16);
					break;
				case Int8:
					SendMessage(hSpinControl, UDM_SETRANGE32, CHAR_MIN, CHAR_MAX);
					SendMessage(hSpinControl, UDM_SETPOS32, 0, VarCurr.Int8);
					break;
				case uInt32:
					SendMessage(hSpinControl, UDM_SETRANGE32, 0, ULONG_MAX);
					SendMessage(hSpinControl, UDM_SETPOS32, 0, VarCurr.uInt32);
					break;
				case uInt16:
					SendMessage(hSpinControl, UDM_SETRANGE32, 0, USHRT_MAX);
					SendMessage(hSpinControl, UDM_SETPOS32, 0, VarCurr.uInt16);
					break;
				case uInt8:
					SendMessage(hSpinControl, UDM_SETRANGE32, 0, UCHAR_MAX);
					SendMessage(hSpinControl, UDM_SETPOS32, 0, VarCurr.uInt8);
					break;
				}
			}

		} else if (strcmp(VarCurr.publicType, "Checkbox") == 0){

		} else if (strcmp(VarCurr.publicType, "ListBox") == 0){
			//Fetch list box items

		}

		//Free junk in those variables
		safe_free(VarCurr.UUID);
		safe_free(VarCurr.desc);
		safe_free(VarCurr.publicType);
		safe_free(VarCurr.mod);
	}

	//Record line count
	{
		int *LineCount = NULL;
		LineCount = malloc(sizeof(int));
		*LineCount = i;
		SetProp(VarWin, "LineCount", LineCount);
	}

	//Set scroll bar
	{
		SCROLLINFO VarWinScroll;
		VarWinScroll.cbSize = sizeof(SCROLLINFO);
		VarWinScroll.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_DISABLENOSCROLL;

		VarWinScroll.nMin = 0;
		VarWinScroll.nMax = (i * 27) + 14;
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

	/*CreateWindow(
		"LISTBOX", "ListBox!",
		WS_CHILD | WS_VISIBLE,
		BigWinSize.left + 10,
		VarWinSize.bottom + 7,
		75, 23,
		BigWin,
		(HMENU)NULL,
		CURRINSTANCE, NULL
	);*/

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

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_FindClear
 *  Description:  Finds free space in the file for modification.
 *                The given chunk of space is the smallest that
 *                is within the bounds given by the input.
 *                If a mod name is given (not NULL), it will also
 *                claim the free space in the name of that mod.
 * =====================================================================================
 */
struct ModSpace Mod_FindSpace(struct ModSpace input, const char *ModUUID, BOOL Clear){
	struct ModSpace result;
	json_t *out, *row;
	char *IDStr = NULL;

	//Set defaults for result
	CURRERROR = errNOERR;
	result.ID = NULL;
	result.Bytes = NULL;
	result.Start = 0;
	result.End = 0;
	result.FileID = 0;
	result.Len = 0;
	result.Valid = FALSE;

	//Do the finding
	{
		sqlite3_stmt *command;
		const char *query = "SELECT * FROM Spaces WHERE "
		                    "Type = ? AND UsedBy IS NULL AND "
		                    "Len >= ? AND Start >= ? AND End <= ? "
		                    "AND File = ? ORDER BY Len;";
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1,
				Clear ? "Clear":"Add", -1, SQLITE_STATIC) ||
			sqlite3_bind_int(command, 2, input.Len) ||
			sqlite3_bind_int(command, 3, input.Start) ||
			sqlite3_bind_int(command, 4, input.End) ||
			sqlite3_bind_int(command, 5, input.FileID)
		) != 0){CURRERROR = errCRIT_DBASE; return result;}

		out = SQL_GetJSON(command);

		if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return result;
		}
	}

	//DEBUG
	/*{
		const char *query = "SELECT * FROM Spaces WHERE "
		                    "Type = '%s' AND UsedBy IS NULL AND "
		                    "Len >= %d AND Start >= %d AND End <= %d "
		                    "AND File = %d ORDER BY Len;\n";
		char *msg = NULL;
		asprintf(
			&msg, query,
			Clear ? "Clear":"Add", input.Len,
			input.Start, input.End, input.FileID
		);
		AlertMsg(msg, "SQL OUTPUT");
		safe_free(msg);
	}*/

	//Make sure json output is valid
	if(!json_is_array(out)){
		CURRERROR = errCRIT_DBASE;
		return result;
	}
	//Make sure there is a space
	row = json_array_get(out, 0);
	if (!json_is_object(row)){
		return result;	//No error; no space
	}

	//Pull the data
	IDStr = JSON_GetStr(row, "ID");
	if(CURRERROR != errNOERR){safe_free(IDStr); return result;}

	result.FileID = JSON_GetInt(row, "File");
	if(CURRERROR != errNOERR){safe_free(IDStr); return result;}
	result.Start = JSON_GetInt(row, "Start");
	if(CURRERROR != errNOERR){safe_free(IDStr); return result;}
	result.End = JSON_GetInt(row, "End");
	if(CURRERROR != errNOERR){safe_free(IDStr); return result;}
	result.Len = result.End - result.Start;

	//Drop if we need to.
	if (ModUUID != NULL){
		char *NewID = NULL;
		sqlite3_stmt *command;
		const char *query = "UPDATE Spaces SET UsedBy = ?, ID = ? "
		                    "WHERE ID = ? AND Type = ?";

		//Is this scalable? I hope it is.
		asprintf(&NewID, "%s~old", IDStr);

		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT) ||
			sqlite3_bind_text(command, 2, NewID, -1, SQLITE_TRANSIENT) ||
			sqlite3_bind_text(command, 3, IDStr, -1, SQLITE_TRANSIENT) ||
			sqlite3_bind_text(command, 4, Clear ? "Clear":"Add", -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
		) != 0){
			safe_free(NewID);
			CURRERROR = errCRIT_DBASE;
			return result;
		}
		command = NULL;
		safe_free(NewID);
	}

	//DO NOT FREE IDStr
	result.ID = IDStr;

	json_decref(out);
	result.Valid = TRUE;
	return result;
}

char * Mod_FindPatchOwner(const char *PatchUUID)
{
	char *out = NULL;
	sqlite3_stmt *command;
	const char *query = "SELECT UUID FROM Spaces "
	                    "JOIN Mods ON Spaces.Mod = Mods.UUID "
	                    "WHERE Spaces.ID = ?";
	CURRERROR = errNOERR;
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return "";}

	out = SQL_GetStr(command);
	if(CURRERROR != errNOERR){
		safe_free(out);
		return "";
	}
	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		safe_free(out);
		return "";
	}
	return out;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  ClearSpace
 *  Description:  Finds and creates an Add space for the given input.
 * =====================================================================================
 */
BOOL ModOp_Reserve(struct ModSpace input, const char *ModUUID){
	//Find some free space to put it in
	struct ModSpace FreeSpace = Mod_FindSpace(input, ModUUID, TRUE);

	if (FreeSpace.Valid == FALSE){ //If no free space
		if(CURRERROR == errNOERR){
			AlertMsg ("There is not enough space cleared in the file to install\n"
			          "this mod. Try removing some mods or installing some\n"
			          "space-clearing mods to help make room.\n"
			          "The installation cannot continue.",
			          "Mod Installation Error");
		}
		return FALSE;
	}
	//Replace the ID of the FreeSpace with the input's ID
	FreeSpace.ID = strdup(input.ID);

	//Make an add space
	if(Mod_AddSpace(FreeSpace, input.Start, ModUUID) == FALSE){
		return FALSE;
	}

	return TRUE;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  ClearSpace
 *  Description:  Creates an clear space marker given a ModSpace struct containing a
 *                start address, end address and file. Very dumb in operation; make sure
 *                space is indeed ready to be cleared first.
 * =====================================================================================
 */
BOOL ModOp_Clear(struct ModSpace input, const char *ModUUID){
	struct ModSpace Occ;
	char *OccOwner;
	CURRERROR = errNOERR;

	//Make sure there isn't anything in the way, first.
	Occ = Mod_FindSpace(input, ModUUID, FALSE);
	if(Occ.Valid == TRUE){ //Something found
		OccOwner = NULL;
	} else { //Nothing found
		OccOwner = Mod_FindPatchOwner(Occ.ID);
	}
	if(CURRERROR != errNOERR){return FALSE;}

	//Check if space is occupied
	if(!OccOwner){OccOwner = strdup("");} //Just in case
	if (strcmp(OccOwner, "") != 0){
		//Check owning mod
		char *errormsg = NULL;
		asprintf(
			&errormsg,
			"The mod is requesting to clear space that is\n"
			"currently occupied by patch %s.\n\n"
			"The installation cannot continue.\n"
			"Remove the offending mod and try again.",
			OccOwner
		);
		AlertMsg (errormsg, "Mod Installation Error");

		safe_free(errormsg);
		safe_free(OccOwner);
		return FALSE;
	}

	//Mark space as used
	{
		sqlite3_stmt *command;
		const char *query = "INSERT INTO Spaces "
				    "(ID, Type, File, Mod, Start, End, Len) "
				    "VALUES (?, 'Clear', ?, ?, ?, ?, ?);";
		char *ID = NULL;

		/* Create ID if not existing */
		if (Occ.ID){
			ID = strdup(Occ.ID);
		} else { //Autogen ID from start pos
			asprintf(&ID, "%d-%X", input.FileID, input.Start);
		}

		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, ID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_int(command, 2, input.FileID) |
			sqlite3_bind_text(command, 3, ModUUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_int(command, 4, input.Start) |
			sqlite3_bind_int(command, 5, input.End) |
			sqlite3_bind_int(command, 6, input.Len)
		) != 0 || SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
		) != 0){
			safe_free(ID);
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		command = NULL;

		safe_free(ID);
	}

	safe_free(OccOwner);
	return TRUE;
}


/*
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_AddSpace
 *  Description:  Creates an occupied space marker given a free space input, minimum
 *                start address and the length needed. A clear space is generated for
 *                any leftover address ranges.
 * =====================================================================================
 */
BOOL Mod_AddSpace(struct ModSpace input, int start, const char *ModUUID){
	//Compute the wanted Start address and End address
	//Tries to go as low as possible, but sometimes the ranges clash.
	int StartAddr = start > input.Start ? start : input.Start;
	int EndAddr = (StartAddr + input.Len) < input.End ? (StartAddr + input.Len) : input.End;
	sqlite3_stmt *command;
	const char *query = "INSERT INTO Spaces "
		"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len') VALUES "
		"(?, ?, ?, ?, ?, ?, ?);";
	CURRERROR = errNOERR;

	/*{
		const char *query = "INSERT INTO Spaces "
		"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len') VALUES "
		"('%s', '%s', '%d', '%s', '%d', '%d', '%d');";
		char *msg = NULL;
		asprintf(&msg, query, input.ID, "Add", input.FileID, ModUUID, StartAddr, EndAddr, input.Len);
		AlertMsg(msg, "SQL OUTPUT");
		safe_free(msg);
	}*/

	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}

	//Create new AddSpc table row
	if(SQL_HandleErrors(
		sqlite3_bind_text(command, 1, input.ID, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 2, "Add", -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 3, input.FileID) |
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_int(command, 5, StartAddr) |
		sqlite3_bind_int(command, 6, EndAddr) |
		sqlite3_bind_int(command, 7, input.Len)
	) != 0 || SQL_HandleErrors(sqlite3_step(command)
	) != 0 || SQL_HandleErrors(sqlite3_reset(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	//Create a ClearSpc entry from [input.start] to [StartAddr] if not equal
	if (input.Start != StartAddr){
		char *inputStartBuff = NULL;
		asprintf(&inputStartBuff, "%s.StartBuff", input.ID);
		if(SQL_HandleErrors(
			sqlite3_bind_text(command, 1, inputStartBuff, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 2, "Clear", -1, SQLITE_STATIC) |
			sqlite3_bind_int(command, 5, input.Start) |
			sqlite3_bind_int(command, 6, StartAddr) |
			sqlite3_bind_int(command, 7, StartAddr - input.Start)
		) != 0 || SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_reset(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			safe_free(inputStartBuff);
			return FALSE;
		}
		safe_free(inputStartBuff);
	}

	//Create a ClearSpc entry from EndAddr to [input.end] if not equal
	if (input.End != EndAddr){
		char *inputEndBuff = NULL;
		asprintf(&inputEndBuff, "%s.EndBuff", input.ID);
		if(SQL_HandleErrors(
			sqlite3_bind_text(command, 1, inputEndBuff, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 2, "Clear", -1, SQLITE_STATIC) |
			sqlite3_bind_int(command, 5, input.End) |
			sqlite3_bind_int(command, 6, EndAddr) |
			sqlite3_bind_int(command, 7, input.End - EndAddr)
		) != 0 || SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_reset(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			safe_free(inputEndBuff);
			return FALSE;
		}
		safe_free(inputEndBuff);
	}

	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	command = NULL;
	return TRUE;
}

/*void MergeFreeSpace(){

	//Best way to do this is to go ahead and produce a mod with Mov operations
	//that can be uninstalled like any other mod.

	For every file id:
		struct ModSpace[] resultArray =
		SELECT * FROM ClearSpc WHERE
		'FileID' = [fileid]
		ORDER BY 'Start';

		Look at next ClearSpcID.
		See if first END is more than next START.
		If so, combine the two

}*/

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  ModOp_Add
 *  Description:  Add operation for mod installation.
 *                Applies to "Add", "Repl" and "Copy" operations.
 *                Finds free space in given file and writes given bytes to it.
 * =====================================================================================
 */
BOOL ModOp_Add(struct ModSpace input, int handle, const char *ModUUID){
	struct ModSpace FreeSpace;

	//Find some free space to put it in
	FreeSpace = Mod_FindSpace(input, ModUUID, TRUE);

	if (FreeSpace.Valid == FALSE){ //If no free space
		if(CURRERROR == errNOERR){
			AlertMsg ("There is not enough space cleared in the file to install\n"
			          "this mod. Try removing some mods or installing some\n"
			          "space-clearing mods to help make room.\n"
			          "The installation cannot continue.",
			          "Mod Installation Error");
		}
		return FALSE;
	}

	//Make an add space
	if(Mod_AddSpace(FreeSpace, input.Start, ModUUID) == FALSE){
		return FALSE;
	}

	//Read existing bytes into revert table and write new bytes in
	{
		unsigned char *OldBytesRaw = NULL;
		char *OldBytes = NULL;
		sqlite3_stmt *command;
		const char *query = "INSERT INTO Revert "
			"('PatchUUID', 'OldBytes') VALUES (?, ?);";

		//Read the old bytes
		OldBytesRaw = calloc(input.Len, sizeof(char));
		//Quick error checking
		if(OldBytesRaw == NULL){
			AlertMsg("Malloc Fail #9", "");
			CURRERROR = errCRIT_MALLOC;
			return FALSE;
		}

		if (read(handle, OldBytesRaw, input.Len) != input.Len){
			AlertMsg("Unexpected amount of bytes read in ModOp_Add.\n"
				"Please report this to mod loader developer.",
				"Mod Loader error");
			safe_free(OldBytesRaw);
			CURRERROR = errCRIT_FUNCT;
			return FALSE;
		}

		//Convert to hex string
		OldBytes = Bytes2Hex(OldBytesRaw, input.Len);
		safe_free(OldBytesRaw);

		//Construct & Execute SQL Statement
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, FreeSpace.ID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 2, OldBytes, -1, SQLITE_TRANSIENT)
		) != 0 || SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			safe_free(OldBytes);
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		command = NULL;

		//Free memory used
		safe_free(OldBytes);
	}

	//File_WriteBytes(handle, FreeSpace.Start, bytes, len+1);
	return TRUE;
}

//WILL LEAK MEMORY IF UNATTENDED
//On failure, output guaranteed to be returned without calls to free() being required
struct ModSpace Mod_GetPatchInfo(json_t *patchCurr, int fhandle)
{
	struct ModSpace input; //Return value
	char *Mode = NULL; //Patch mode
	CURRERROR = errNOERR;
	input.Valid = FALSE;

	//Get Mode
	Mode = JSON_GetStr(patchCurr, "Mode");
	if(strcmp(Mode, "") == 0){
		//Mode was not found
		AlertMsg("`Mode` not defined for patch.", "JSON Error");
		goto Mod_GetBytes_Failure;
	}

	///Set FileID
	{
		char *FileName = JSON_GetStr(patchCurr, "File");
		if(strcmp(FileName, "") == 0){
			AlertMsg("`File` not defined for patch.", "JSON Error");
			goto Mod_GetBytes_Failure;
		}
		input.FileID = File_GetID(FileName);
		safe_free(FileName);
	}
	//Init ID to NULL for later use
	input.ID = NULL;

	///Set Start, End, Len
	//Make sure Start exists
	{
		char *temp = JSON_GetStr(patchCurr, "Start");
		if(temp == NULL){
			//Start was not found
			AlertMsg("`Start/End` not defined for patch.", "JSON Error");
			goto Mod_GetBytes_Failure;
		}
		safe_free(temp);
	}
	if (!json_is_integer(json_object_get(patchCurr, "Start"))){ //Start is UUID
		char *UUID = JSON_GetStr(patchCurr, "Start");
		//Replace Start and End with UUID's start/end
		Mod_FindUUIDLoc(&input.Start, &input.End, UUID);

		//If we're doing a replacement, make the new patch
		//take the UUID of the old space.
		if(strcmp(Mode, "Repl") == 0){
			input.ID = strdup(UUID);
		}
	} else { //Start is a number
		input.Start = JSON_GetInt(patchCurr, "Start");
		input.End = JSON_GetInt(patchCurr, "End") + 1;
	}
	input.Len = input.End - input.Start;

	///Set ID (if unset by REPL start/end code)
	//Requires Start, FileID
	if(input.ID == NULL){
		char *UUID = JSON_GetStr(patchCurr, "UUID");

		//If there was no UUID, make one up. (Hex representation of start)
		if(strcmp(UUID, "") == 0){
			safe_free(UUID);
			asprintf(&input.ID, "%d-%X", input.FileID, input.Start);
		} else {
			input.ID = UUID;
		}
	}

	///Set Bytes, Len, SrcStart, SrcEnd (if applicable)
	//Copy/Move
	if(strcmp(Mode, "Move") == 0 || strcmp(Mode, "Copy") == 0){
		//Make sure SrcStart exists
		{
			char *temp = JSON_GetStr(patchCurr, "SrcStart");
			if(temp == NULL){
				AlertMsg("`SrcStart/SrcEnd` not defined for patch.", "JSON Error");
				goto Mod_GetBytes_Failure;
			}
			safe_free(temp);
		}
		if (!json_is_integer(json_object_get(patchCurr, "SrcStart"))){ //UUID
			char *UUID = JSON_GetStr(patchCurr, "Start");
			//Replace Start and End with UUID's start/end
			Mod_FindUUIDLoc(&input.SrcStart, &input.SrcEnd, UUID);

			//If we're doing a replacement, make the new patch
			//take the UUID of the old space. (again)
			if(strcmp(Mode, "Repl") == 0){
				safe_free(input.ID);
				input.ID = strdup(UUID);
			}
		} else { //Start is a number
			input.SrcStart = JSON_GetInt(patchCurr, "SrcStart");
			input.SrcEnd = JSON_GetInt(patchCurr, "SrcEnd") + 1;
		}

		input.Len = input.SrcEnd - input.SrcStart;

		//Allocate space for bytes
		input.Bytes = calloc(input.Len, sizeof(char));
		if(input.Bytes == NULL){
			AlertMsg("Malloc Fail #10", "");
			CURRERROR = errCRIT_MALLOC;
			goto Mod_GetBytes_Failure;
		}

		//Set value of Bytes to value of bytes from SrcStart to SrcEnd
		lseek(fhandle, input.SrcStart, SEEK_SET);
		if (read(fhandle, input.Bytes, input.Len) != input.Len){
			safe_free(input.Bytes);
			CURRERROR = errCRIT_FUNCT;
			goto Mod_GetBytes_Failure;
		}

		//Return without failure (Yes, this was necessary.)
		input.Valid = TRUE;
		goto Mod_GetBytes_Return;
	}

	//Add, Repl
	else if(strcmp(Mode, "Add") == 0 || strcmp(Mode, "Repl") == 0){
		char *ByteStr = NULL, *ByteMode = NULL;

		//Get Byte Mode and String.
		ByteStr = JSON_GetStr(patchCurr, "Value");
		ByteMode = JSON_GetStr(patchCurr, "AddType");
		if(CURRERROR != errNOERR){
			goto Mod_GetBytes_Failure;
		}

		//Error checking
		if (strcmp(ByteMode, "") == 0){
			CURRERROR = errWNG_MODCFG;
			goto Mod_GetBytes_Failure;
		}

		///Possible byte modes:
		//  "Bytes"       - Raw data in hex form to be copied exactly.
		//                  Use for very short snippets of data or code.
		//  "Path"        - Path to file inside mod's folder.
                //                  Use for resources or large code samples.
		//  "UUIDPointer" - Give it the GUID of a space and it will give the
                //                  allocated memory address. For subroutines, etc.
		//  "VarValue"    - Give it the GUID of a mod loader variable
		//                  and it will give it's contents.
		if (strcmp(ByteMode, "Bytes") == 0){
			//Convert the hex value of Value to bytes
			input.Bytes = Hex2Bytes(ByteStr, &input.Len);
		}

		if (strcmp(ByteMode, "UUIDPointer") == 0){
			int start, end; //int MUST be 4 bytes.
			//Find address/length of ID
			Mod_FindUUIDLoc(&start, &end, ByteStr);
			input.Len = 4; //Size of 32-bit ptr.

			//Allocate space for bytes
			input.Bytes = calloc(4, sizeof(char));
			if(input.Bytes == NULL){
				AlertMsg("Malloc Fail #11", "");
				CURRERROR = errCRIT_MALLOC;
				goto Mod_GetBytes_Failure;
			}

			//Set input.Bytes to value of start
			memcpy(input.Bytes, &start, 4);
		}

		//Return without failure (Yes, this was necessary.)
		input.Valid = TRUE;
		goto Mod_GetBytes_Return;
	}

	input.Valid = TRUE; //We reached end naturally, success
	goto Mod_GetBytes_Return;

Mod_GetBytes_Failure:
	safe_free(Mode);
	/*safe_free(input.ID);
	safe_free(input.Mode);
	safe_free(input.Bytes);*/

Mod_GetBytes_Return:
	return input;
}

//Replace the info with the start/end of the given UUID
BOOL Mod_FindUUIDLoc(int *start, int *end, const char *UUID)
{
	sqlite3_stmt *command;
	json_t *out, *row;
	const char *query = "SELECT Start, End FROM Spaces "
		     "WHERE ID = ? AND UsedBy IS NULL;";

	CURRERROR = errNOERR;

	//Get the list
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}

	out = SQL_GetJSON(command);
	row = json_array_get(out, 0);

	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}

	//Make sure list is not empty
	if(json_array_size(out) == 0){
		char *errormsg = NULL;
		asprintf(
			&errormsg, "%sThe UUID %s is not a known UUID from\n"
			           "the game or any installed mods.",
			errormsg_ModInst_CantCont, UUID
		);
		AlertMsg(errormsg, "Mod Metadata Error");
		safe_free(errormsg);

		CURRERROR = errWNG_MODCFG;
		return FALSE;
	}

	//Parse the list
	*start = JSON_GetInt(row, "Start");
	*end = JSON_GetInt(row, "End");

	json_decref(out);
	return TRUE;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_Install
 *  Description:  Parses array of patches and adds/removes bytes depending on values.
 *                Patches support 6 operations, defined in manual.
 * =====================================================================================
 */
BOOL Mod_Install(json_t *root)
{
	json_t *patchArray, *patchCurr;
	size_t i;
	char *ModUUID = JSON_GetStr(root, "UUID");
	BOOL retval = TRUE;

	//Define system-specific progress dialog
	#ifdef _WIN32
	HWND ProgDialog;
	#endif

	//Clear errors
	CURRERROR = errNOERR;

	patchArray = json_object_get(root, "patches");
	if(!patchArray || !json_is_array(patchArray)){
		AlertMsg("No patches to install.", "");
		Mod_AddToDB(root);
		return TRUE;
	}
	ProgDialog = ProgDialog_Init(
		json_array_size(patchArray),
		"Installing Mod..."
	);
	if(CURRERROR != errNOERR){
		ProgDialog_Kill(ProgDialog);
		return FALSE;
	}

	json_array_foreach(patchArray, i, patchCurr){
		char *Mode = NULL;
		int file = -1;
		unsigned char *bytes;
		struct ModSpace input;

		//Change directory back to game's root in case it was changed.
		chdir(CURRDIR);
		ErrNo2ErrCode();
		if(CURRERROR != errNOERR){
			retval = FALSE;
			goto Mod_Install_Cleanup;
		}

		//Open this patch's file
		file = File_OpenSafe(JSON_GetStr(patchCurr, "File"), _O_BINARY|_O_RDWR);
		if(file == -1){
			AlertMsg("`File` is not the path to a valid file.", "");
			retval = FALSE;
			goto Mod_Install_Cleanup;
		}

		//Get patch mode
		Mode = JSON_GetStr(patchCurr, "Mode");
		if(strcmp(Mode, "") == 0){
			//Mode was not found
			AlertMsg("`Mode` not defined for patch.", "JSON Error");
			retval = FALSE;
			goto Mod_Install_Cleanup;
		}

		//Fill input struct
		input = Mod_GetPatchInfo(patchCurr, file);
		if(input.Valid == FALSE){
			AlertMsg("GetPatchInfo failed...", "");
			retval = FALSE;
			goto Mod_Install_Cleanup;
		}

		///Apply patches
		//Replace
		if(strcmp(Mode, "Repl") == 0){
			retval = ModOp_Clear(input, ModUUID) |
			ModOp_Add(input, file, ModUUID);
			safe_free(input.Bytes);
		//Clear
		} else if(strcmp(Mode, "Clear") == 0){
			retval = ModOp_Clear(input, ModUUID);
		//Add or Copy (Copy sets bytes beforehand to emulate Add)
		} else if(strcmp(Mode, "Add") == 0 || strcmp(Mode, "Copy") == 0){
			retval = ModOp_Add(input, file, ModUUID);
			safe_free(input.Bytes);
		//Reserve
		} else if(strcmp(Mode, "Reserve") == 0){
			retval = ModOp_Reserve(input, ModUUID);
		//Move
		} else if(strcmp(Mode, "Move") == 0){
			//This is gonna get kinda weird.
			//Make a new modspace representing the source chunk
			struct ModSpace srcInput = input;
			srcInput.Start = srcInput.SrcStart;
			srcInput.End = srcInput.SrcEnd;

			//Remove the source
			retval = ModOp_Clear(srcInput, ModUUID);
			if(retval == FALSE){break;}

			//Add new one in new location
			retval = ModOp_Add(input, file, ModUUID); //Add to Dest

		//Unknown mode
		} else {
			AlertMsg("Unknown patch mode", Mode);
			retval = FALSE;
		}
		close(file);
		safe_free(input.ID);
		ProgDialog_Update(ProgDialog, 1);

		if(retval == FALSE || CURRERROR != errNOERR){
			goto Mod_Install_Cleanup;
		}
	}

	//Add variable entries
	{
		json_t *varArray, *varCurr;
		varArray = json_object_get(root, "variables");
		if(!patchArray || !json_is_array(patchArray)){
			goto Mod_Install_Cleanup;
		}
		json_array_foreach(varArray, i, varCurr){
			Var_MakeEntry(varCurr, ModUUID);
		}
	}

Mod_Install_Cleanup:
	Mod_AddToDB(root);
	safe_free(ModUUID);
	if(CURRERROR != errNOERR){retval = FALSE;}
	ProgDialog_Kill(ProgDialog);
	return retval;
}

//Add mod to database
BOOL Mod_AddToDB(json_t *root){
	sqlite3_stmt *command;
	char *uuid = JSON_GetStr(root, "UUID");
	char *name = JSON_GetStr(root, "Name");
	char *desc = JSON_GetStr(root, "Desc");
	char *auth = JSON_GetStr(root, "Auth");
	char *date = JSON_GetStr(root, "Date");
	char *cat = JSON_GetStr(root, "Cat");
	int ver = JSON_GetInt(root, "Ver");

	const char *query = "INSERT INTO Mods "
		"('UUID', 'Name', 'Desc', 'Auth', 'Ver', 'Date', 'Cat') VALUES "
		"(?, ?, ?, ?, ?, ?, ?);";
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_text(command, 1, uuid, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 2, name, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 3, desc, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 4, auth, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_int(command, 5, ver) |
		sqlite3_bind_text(command, 6, date, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 7, cat, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}

	if(SQL_HandleErrors(sqlite3_step(command)
	) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}
	command = NULL;

	safe_free(uuid);
	safe_free(name);
	safe_free(desc);
	safe_free(auth);
	safe_free(date);
	safe_free(cat);
	return TRUE;
}

BOOL Mod_FindDep(const char *ModUUID)
{
	int modCount;
	sqlite3_stmt *command;
	const char *query = "SELECT COUNT(*) FROM Dependencies "
		"JOIN Mods ON Dependencies.ParentUUID = Mods.UUID "
		"WHERE Dependencies.ChildUUID = ?";

	CURRERROR = errNOERR;
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}

	modCount = SQL_GetNum(command);
	if(CURRERROR != errNOERR){return FALSE;}
	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}

	return modCount != 0 ? TRUE : FALSE;
}

void Mod_FindDepAlert(const char *ModUUID){
	char *errormsg = NULL;
	sqlite3_stmt *command;
	unsigned int i, errorlen;
	json_t *out, *row;
	const char *query = "SELECT Name, Auth, Ver FROM Dependencies "
	                    "JOIN Mods ON Dependencies.ParentUUID = Mods.UUID "
	                    "WHERE Dependencies.ChildUUID = ?;";
	CURRERROR = errNOERR;

	//Get dependency list
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){CURRERROR = errCRIT_DBASE; return;}
	if(SQL_HandleErrors(
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return;}
	out = SQL_GetJSON(command);
	if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return;
	}
	command  = NULL;

	errormsg = strdup("Uninstalling this mod will require "
	                 "you to uninstall the following:\n\n");
	errorlen = strlen(errormsg);

	json_array_foreach(out, i, row){
		char *temp = NULL;
		int templen;

		//Print it into a string
		templen = asprintf(&temp, "- %s version %s by %s\n",
			JSON_GetStr(row, "Name"),
			JSON_GetStr(row, "Ver"),
			JSON_GetStr(row, "Auth")
		);

		while(strlen(errormsg) + templen + 1 >= errorlen){
			errorlen *= errorlen;
			errormsg = realloc(errormsg, errorlen);
			if(errormsg == NULL){
				AlertMsg("Malloc Fail #1", "");
				CURRERROR = errCRIT_MALLOC;
				safe_free(temp);
				json_decref(out);
				return;
			}
		}

		//Add it to the error message
		strcat(errormsg, temp);
		safe_free(temp);
	}
	//TODO: Prompt user yes/no, then uninstall in order (not in this funct.)
	AlertMsg(errormsg, "Other Mods Preventing Uninstallation");
	safe_free(errormsg);
	json_decref(out);
	return;
}

//Write back the old bytes and then remove the space.
BOOL ModOp_UnAdd(json_t *row)
{
	int filehandle, offset, datalen;
	unsigned char *bytes;
	char *ByteStr, *PatchUUID;
	CURRERROR = errNOERR;

	//Open file handle
	{
		char *filename = File_GetName(JSON_GetInt(row, "File"));
		if(filename == NULL){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		filehandle = File_OpenSafe(filename, _O_BINARY|_O_RDWR);
		safe_free(filename);
	}
	if(filehandle == -1){
		return FALSE;
	} //Failure.

	//Get offset & length
	offset = JSON_GetInt(row, "Start");

	PatchUUID = JSON_GetStr(row, "ID");

	//Retrieve data
	{
		sqlite3_stmt *command;
		const char *query = "SELECT OldBytes FROM Revert WHERE PatchUUID = ?;";

		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_TRANSIENT)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

		ByteStr = SQL_GetStr(command);
		if(ByteStr == NULL){
			return FALSE;
		}
		if(CURRERROR != errNOERR){
			safe_free(ByteStr);
			return FALSE;
		}

		if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			safe_free(ByteStr);
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		command = NULL;
	}

	//Convert data string from hex to raw bytes
	bytes = Hex2Bytes(ByteStr, &datalen);
	safe_free(ByteStr);

	//Write back data
	//File_WriteBytes(filehandle, offset, bytes, datalen);

	safe_free(bytes);
	close(filehandle);

	//Remove the space and the old bytes
	ModOp_UnClear(row); //Efficiency!
	{
		sqlite3_stmt *command;
		const char *query = "DELETE FROM Revert WHERE PatchUUID = ?;";

		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_TRANSIENT)
		) != 0 || SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		command = NULL;
	}

	return TRUE;
}

//Just remove the space. (We are uninstalling sequentially for a reason.)
BOOL ModOp_UnClear(json_t *row)
{
	sqlite3_stmt *command;
	char *PatchUUID = JSON_GetStr(row, "ID");
	const char *query = "DELETE FROM Spaces WHERE ID = ?;";
	CURRERROR = errNOERR;

	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_TRANSIENT)
	) != 0 || SQL_HandleErrors(sqlite3_step(command)
	) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		safe_free(PatchUUID);
		return FALSE;
	}
	command = NULL;

	safe_free(PatchUUID);
	return TRUE;
}

BOOL Mod_Uninstall(const char *ModUUID)
{
	int modCount, i, modStop;
	BOOL retval = TRUE;
	//Define progress dialog (handle is shell-specific)
	#ifdef _WIN32
	HWND ProgDialog;
	#endif

	CURRERROR = errNOERR;

	chdir(CURRDIR); //In case path was changed during installation or w/e.

	//Check for dependencies
	if(Mod_FindDep(ModUUID) == TRUE){
		Mod_FindDepAlert(ModUUID);
		return FALSE;
	};

	//Count mods
	{
		sqlite3_stmt *command;
		const char *query = "SELECT COUNT(*) FROM Mods;";
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
		modCount = SQL_GetNum(command);
		if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
	}

	//Get stopping point (Uninstall everything up to selected mod)
	{
		sqlite3_stmt *command;
		const char *query = "SELECT RowID FROM Mods WHERE UUID = ?;";

		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}

		modStop = SQL_GetNum(command);
		if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
	}

	//Record source folders of all mods to be uninstalled

	//Init progress box
	ProgDialog = ProgDialog_Init(modCount, "Uninstalling Mod...");

	//Perform operations in reverse from newest mod to selected mod
	for (i = modCount; i >= modStop; i--){
		json_t *out, *row;
		unsigned int j;
		char *CurrUUID = NULL;

		//Get UUID of current mod
		{
			sqlite3_stmt *command;
			const char *query = "SELECT UUID FROM Mods WHERE RowID = ?;";

			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_int(command, 1, i)
			) != 0){
				CURRERROR = errCRIT_DBASE;
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
			//Get UUID
			CurrUUID = SQL_GetStr(command);
			//Finalize
			sqlite3_finalize(command);
			command = NULL;
		}

		//Select every patch created by current mod
		{
			sqlite3_stmt *command;
			const char *query = "SELECT * FROM Spaces WHERE Mod = ?";

			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_text(command, 1, CurrUUID, -1, SQLITE_TRANSIENT)
			) != 0){
				CURRERROR = errCRIT_DBASE;
				retval = FALSE;
				safe_free(CurrUUID);
				goto Mod_Uninstall_Cleanup;
			}

			out = SQL_GetJSON(command);
			sqlite3_finalize(command);
			command = NULL;
		}

		//Loop through patches
		json_array_foreach(out, j, row){
			//Find type of patch
			char *SpaceType = JSON_GetStr(row, "Type");

			//Patch type is Add
			if(strcmp(SpaceType, "Add") == 0){
				if(ModOp_UnAdd(row) == FALSE){
					retval = FALSE;
					safe_free(CurrUUID);
					goto Mod_Uninstall_Cleanup;
				}
			//Patch type is Clear
			} else if (strcmp(SpaceType, "Clear") == 0) {
				if(ModOp_UnClear(row) == FALSE){
					retval = FALSE;
					safe_free(CurrUUID);
					goto Mod_Uninstall_Cleanup;
				}
			}
			safe_free(SpaceType);
		}
		json_decref(out);
		out = NULL;

		//Mod has now been uninstalled.

		//Get rid of "~old" at the end of spaces used by this mod
		{
			sqlite3_stmt *command;
			//Remove any instances of "~old" in ID
			const char *query = "UPDATE Spaces SET ID = REPLACE(ID, '~old', '') "
			                    "WHERE UsedBy = ?;";
			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_text(command, 1, CurrUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(sqlite3_step(command)
			) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
				safe_free(CurrUUID);
				CURRERROR = errCRIT_DBASE;
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
			command = NULL;
		}

		//Unmark used spaces
		{
			sqlite3_stmt *command;
			//Clear UsedBy if equal to UUID of current mod
			const char *query = "UPDATE Spaces SET UsedBy = NULL WHERE UsedBy = ?;";
			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_text(command, 1, CurrUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(sqlite3_step(command)
			) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
				safe_free(CurrUUID);
				CURRERROR = errCRIT_DBASE;
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
			command = NULL;
		}

		//Remove dependencies
		{
			sqlite3_stmt *command;
			const char *query = "DELETE FROM Dependencies WHERE ParentUUID = ?;";
			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(sqlite3_step(command)
			) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
				safe_free(CurrUUID);
				CURRERROR = errCRIT_DBASE;
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
			command = NULL;
		}


		//Remove mod entry
		{
			sqlite3_stmt *command;
			const char *query = "DELETE FROM Mods WHERE UUID = ?;";
			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_text(command, 1, CurrUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(sqlite3_step(command)
			) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
				safe_free(CurrUUID);
				CURRERROR = errCRIT_DBASE;
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
			command = NULL;
		}

		//Remove mod variables
		Var_ClearEntry(CurrUUID);

		//Free current UUID
		safe_free(CurrUUID);

		//Increment progress bar
		ProgDialog_Update(ProgDialog, 1);
	}

Mod_Uninstall_Cleanup:
	//Kill progress box
	ProgDialog_Kill(ProgDialog);

	//Reinstall existing mods

	//Return
	return retval;
}

//TODO: This is really Win32-specific. Fix that.
BOOL Mod_InstallPrep(HWND hwnd){
	json_t *jsonroot;
	char *fpath = NULL; // Path mod is located at
	char *fname = NULL; // Filename of mod
	int nameOffset;
	BOOL retval = TRUE;

	CURRERROR = errNOERR;

	//Select mod
	fpath = SelectFile(hwnd, &nameOffset, "Mod Metadata (.json)\0*.json\0\0");
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
	fname = fpath + nameOffset;

	/// Copy mod to [path]/mods/[name]
	// Set destination
	// Actually copy w/ Windows API

	///Load
	jsonroot = JSON_Load(fpath);
	if (jsonroot == NULL){CURRERROR = errWNG_MODCFG; return FALSE;}

	///Verify
	if (Mod_CheckCompat(jsonroot) == FALSE){
		ErrCracker(CURRERROR); //Catch errors
		retval = FALSE;
		goto Mod_InstallPrep_Cleanup;
	}
	if (Mod_CheckConflict(jsonroot) != GO){
		ErrCracker(CURRERROR); //Catch errors
		retval = FALSE;
		goto Mod_InstallPrep_Cleanup;
	}
	if (Mod_CheckDep(jsonroot) == FALSE){
		if(CURRERROR == errNOERR){ //No error, just collision
			Mod_CheckDepAlert(jsonroot);
		} else {
			ErrCracker(CURRERROR); //Catch errors
		}
		retval = FALSE;
		goto Mod_InstallPrep_Cleanup;
	}

	///Install mod
	//Disable window during install
	EnableWholeWindow(hwnd, FALSE);

	//Create SQL transaction
	//sqlite3_exec(CURRDB, "BEGIN TRANSACTION", NULL, NULL, NULL);

	if (Mod_Install(jsonroot) == FALSE){
		char *ModUUID = NULL;
		ErrCracker(CURRERROR); //Catch errors

		ModUUID = JSON_GetStr(jsonroot, "UUID");
		AlertMsg("Installation failed. Rolling back...", "Installation Failure");
		//Shoot. We'd need to rollback the binary writes too.
		//Uh, scrap this. Just uninstall normally.
		//sqlite3_exec(CURRDB, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
		Mod_Uninstall(ModUUID);
		safe_free(ModUUID);

		retval = FALSE;
		goto Mod_InstallPrep_Cleanup;
	};

	//End SQL Transaction
	//sqlite3_exec(CURRDB, "COMMIT TRANSACTION", NULL, NULL, NULL);

	//Update mod list
	UpdateModList(GetDlgItem(hwnd, IDC_MAINMODLIST));
	UpdateModDetails(
		hwnd,
		SendMessage(
			GetDlgItem(hwnd, IDC_MAINMODLIST),
			LB_GETCURSEL, 0, 0
		)
	);
	ErrCracker(CURRERROR);

	//Re-enable window & Cleanup
	Mod_InstallPrep_Cleanup:
	safe_free(fpath);
	EnableWholeWindow(hwnd, TRUE);
	json_decref(jsonroot);
	return retval;
}

BOOL Mod_UninstallPrep(HWND hwnd){
	//Get the UUID of the currently selected mod
	char *ModUUID = Mod_GetUUIDFromWin(hwnd);
	if(ModUUID == NULL){return FALSE;}

	//Finally remove the mod
	EnableWholeWindow(hwnd, FALSE);
	Mod_Uninstall(ModUUID);
	ErrCracker(CURRERROR);
	safe_free(ModUUID);

	//Update UI
	UpdateModList(GetDlgItem(hwnd, IDC_MAINMODLIST));
	UpdateModDetails(hwnd, SendMessage(
		GetDlgItem(hwnd, IDC_MAINMODLIST),
		LB_GETCURSEL, 0, 0)
	);
	ErrCracker(CURRERROR);
	EnableWholeWindow(hwnd, TRUE);
	return TRUE;
}

const char *Mod_GetUUIDFromWin(HWND hwnd)
{
	char *ModName = NULL;
	char *ModUUID = NULL;
	int len, CurrSel;
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
		if(SQL_HandleErrors(
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

///Win32 GUI components
//////////////////////////////

// Patch Viewer Dialog Procedure
/*BOOL CALLBACK Dlg_Patch(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	char title[128] = "Patches for file ";
	switch(Message){
	case WM_INITDIALOG:
		//strcat(title, FileIDList[lParam].path);
		strcat(title, "SonicR.EXE");
		SetWindowText(hwnd, title);

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
	default:
		return FALSE;
	}
	return TRUE;
}*/

// About Dialog Procedure
BOOL CALLBACK Dlg_Generic(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
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

//Open database & create tables
BOOL SQL_Load(){
	CURRERROR = errNOERR;
	if(SQL_HandleErrors(
		sqlite3_open("test.db", &CURRDB)
	) != 0 || SQL_HandleErrors(
		sqlite3_exec(CURRDB,
			"CREATE TABLE IF NOT EXISTS 'Spaces'( "
			"`ID`   	TEXT NOT NULL PRIMARY KEY UNIQUE,"
			"`Type`   	TEXT NOT NULL,"
			"`File`   	INTEGER NOT NULL,"
			"`Mod`   	TEXT," //Corresponds with UUID
			"`Start`	INTEGER NOT NULL,"
			"`End`   	INTEGER NOT NULL,"
			"`Len`   	INTEGER,"
			"`UsedBy`	TEXT );",
			NULL, NULL, NULL
		) | sqlite3_exec(CURRDB,
			"CREATE TABLE IF NOT EXISTS `Mods` ("
			"`UUID`	TEXT NOT NULL UNIQUE PRIMARY KEY,"
			"`Name`	TEXT NOT NULL,"
			"`Desc`	TEXT,"
			"`Auth`	TEXT,"
			"`Ver` 	INTEGER NOT NULL,"
			"`Date`	TEXT,"
			"`Cat` 	TEXT );",
			NULL, NULL, NULL
		) | sqlite3_exec(CURRDB,
			"CREATE TABLE IF NOT EXISTS `Dependencies` ("
			"`ParentUUID`	TEXT NOT NULL,"
			"`ChildUUID` 	TEXT NOT NULL);",
			NULL, NULL, NULL
		) | sqlite3_exec(CURRDB,
			"CREATE TABLE IF NOT EXISTS `Revert` ("
			"`PatchUUID`	TEXT NOT NULL,"
			"`OldBytes` 	TEXT NOT NULL);",
			NULL, NULL, NULL
		) | sqlite3_exec(CURRDB,
			"CREATE TABLE IF NOT EXISTS `Files` ("
			"`ID`  	INTEGER NOT NULL,"
			"`Path`	TEXT NOT NULL);",
			NULL, NULL, NULL
		) | sqlite3_exec(CURRDB,
			"CREATE TABLE IF NOT EXISTS `Variables` ("
			"`UUID`       TEXT NOT NULL,"
			"`Mod`        TEXT NOT NULL,"
			"`Type`       TEXT NOT NULL,"
			"`PublicType` TEXT,"
			"`Desc`       TEXT,"
			"`Value`      BLOB);",
			NULL, NULL, NULL
		) | sqlite3_exec(CURRDB,
			"CREATE TABLE IF NOT EXISTS `VarList` ("
			"`Var`     TEXT NOT NULL,"
			"`Number`  INTEGER NOT NULL,"
			"`Label`   TEXT NOT NULL);",
			NULL, NULL, NULL
		)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	sqlite3_extended_result_codes(CURRDB, 1);

	return TRUE;
}

BOOL SQL_Populate(json_t *GameCfg)
{
	json_t *spaceArray, *spaceCurr;
	size_t i;
	int spaceCount;
	CURRERROR = errNOERR;

	//Don't populate if we don't need to
	{
		const char *query = "SELECT COUNT(*) FROM Spaces";
		sqlite3_stmt *command;
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

		spaceCount = SQL_GetNum(command);
		if(CURRERROR != errNOERR){return FALSE;}

		if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		command = NULL;
	}

	//Actually return if there's nothing
	if (spaceCount != 0){return TRUE;}

	//To decrease disk I/O and increase speed
	if(SQL_HandleErrors(
		sqlite3_exec(CURRDB, "BEGIN TRANSACTION", NULL, NULL, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	spaceArray = json_object_get(GameCfg, "KnownSpaces");
	json_array_foreach(spaceArray, i, spaceCurr){
		{
			char *query = "INSERT INTO Spaces "
				"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len')  VALUES"
				"(?, \"Add\", 0, \"\", ?, ?, ?);";
			sqlite3_stmt *command;
			int start = JSON_GetInt(spaceCurr, "Start");
			int end = JSON_GetInt(spaceCurr, "End");

			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(
				sqlite3_bind_text(command, 1, JSON_GetStr(spaceCurr, "UUID"), -1, SQLITE_TRANSIENT) |
				sqlite3_bind_int(command, 2, start) |
				sqlite3_bind_int(command, 3, end) |
				sqlite3_bind_int(command, 4, end-start)
			) != 0 || SQL_HandleErrors(sqlite3_step(command)
			) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
			) != 0){
				CURRERROR = errCRIT_DBASE;
				return FALSE;
			}
			command = NULL;
		}
		{
			const char *query = "DELETE FROM Mods WHERE RowID = ?;";
			sqlite3_stmt *command;

			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(sqlite3_bind_int(command, 1, i)
			) != 0 || SQL_HandleErrors(sqlite3_step(command)
			) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
			) != 0){
				CURRERROR = errCRIT_DBASE;
				return FALSE;
			}
			command = NULL;
		}
	}

	if(SQL_HandleErrors(
		sqlite3_exec(CURRDB, "COMMIT TRANSACTION", NULL, NULL, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	return TRUE;
}

// Main Dialog Procedure
BOOL CALLBACK Dlg_Main(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message){
	case WM_INITDIALOG:{
		//Set program icon
		SendMessage(hwnd, WM_SETICON, ICON_SMALL,
			(LPARAM)LoadImage(GetModuleHandle(NULL),
			MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0));
		SendMessage(hwnd, WM_SETICON, ICON_BIG,
			(LPARAM)LoadIcon(GetModuleHandle(NULL),
			MAKEINTRESOURCE(IDI_ICON)));

		//Update GUI Elements
		UpdateModList(GetDlgItem(hwnd, IDC_MAINMODLIST));

		return TRUE;
	}
	case WM_COMMAND:{
		switch(LOWORD(wParam)){
		case IDCANCEL:
			DestroyWindow(hwnd);
			PostQuitMessage(0);
		break;
		case IDO_ABOUT:
			DialogBoxSysFont(IDD_ABOUT, Dlg_Generic, hwnd);
		break;
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
			ErrCracker(CURRERROR); //Just in case
		break;
		case IDREMOVE:
			Mod_UninstallPrep(hwnd);
			ErrCracker(CURRERROR); //Just in case
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
			ErrCracker(CURRERROR); //Just in case

			//Show window
			ShowWindow(VarWin, SW_SHOWNORMAL);
		break;
		}

		}
	return TRUE;
	}
	default:
		return FALSE;
	}
}

int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInst, char * cmdParam, int cmdShow)
{
	MSG  msg;
	int status;
	HWND hDialog = 0;

	char *tempDir = NULL;
	BOOL canQuit = FALSE;

	// Load program preferences/locations
	json_t *GameCfg = JSON_Load(GAMECONFIG);
	assert(GameCfg != NULL);

	//Set hInst
	CURRINSTANCE = hInst;

	// Select an installation folder
	do{
		tempDir = SelectInstallFolder(GameCfg);
		//Custom error messages/actions
		switch(CURRERROR){
		case errNOERR:
			canQuit = TRUE;
			break;

		case errWNG_BADDIR:
		case errWNG_BADFILE:
			AlertMsg(errormsg_GameDir_BadContents, "Incorrect Directory Contents");
			CURRERROR = errNOERR;
			break;

		case errWNG_CONFIG:
			AlertMsg(errormsg_Cfg_Game, "Configuration File Error");
			CURRERROR = errUSR_QUIT;
			//Fix config file?
			return 2;

		case errCRIT_FILESYS:
			AlertMsg(errormsg_Crit_Sys, "File System Failure!");
			//fall-through
		case errUSR_ABORT:
			CURRERROR = errUSR_QUIT;
			//fall-through
		case errUSR_QUIT:
			return 2;

		default:
			break;
		}
		//Other errors
		ErrCracker(CURRERROR);
	} while (canQuit == FALSE);
	CURRERROR = errNOERR;
	CURRDIR = tempDir;
	//DO NOT free tempDir

	//Define VarWindow class
	{
		WNDCLASS wc = {0};
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

	//Define default font
	{
		NONCLIENTMETRICS *MetricsConfig = malloc(sizeof(NONCLIENTMETRICS));

		MetricsConfig->cbSize = sizeof(NONCLIENTMETRICS);
		SystemParametersInfo(
			SPI_GETNONCLIENTMETRICS,
			MetricsConfig->cbSize,
			MetricsConfig, 0
		);

		DLGFONT = CreateFontIndirect(&(MetricsConfig->lfMessageFont));
		DLGFONTSIZE = (unsigned short)MetricsConfig->lfMessageFont.lfHeight;
	}

	//Load SQL stuff
	SQL_Load();
	ErrCracker(CURRERROR);
	SQL_Populate(GameCfg);
	ErrCracker(CURRERROR);

	///Init main dialog
	hDialog = CreateDialogSysFont(IDD_MAIN, Dlg_Main, NULL);

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
	sqlite3_close(CURRDB);
	return msg.wParam;
}

