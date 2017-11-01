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
        ofn.Flags = OFN_EXPLORER|OFN_DONTADDTORECENT|OFN_ENABLESIZING|OFN_HIDEREADONLY;
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
	ProgDialog = CreateDialog(
		GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDD_PROGRESS),
		0,
		Dlg_Generic
	);
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
				AlertMsg(sqlite3_column_text(stmt, i), "SQL->JSON ERROR!");
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
	if (json_is_string(object)) {
		char temp = JSON_GetStr(root, name);
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
		char temp = JSON_GetStr(root, name);
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
 *                Code by Micheal Foukarakis on StackOverflow
 * =====================================================================================
 */
/*unsigned char * Hex2Bytes(char *hexstring, int len)
{
	char *pos = hexstring;
	unsigned char * val;
	int i = 0;
	val = malloc(len);
	//WARNING: no sanitization or error-checking whatsoever
	for(i = 0; i <= len; i++) {
		sscanf(pos, "%2hhx", &val[i]);
		pos += 2;
	}
	return val;
}*/

//result (input) is a null pointer
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
			if(buf == NULL){safe_free(name);
				CURRERROR = errCRIT_MALLOC; return ERR;}
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
			if(buf == NULL){safe_free(name);
				CURRERROR = errCRIT_MALLOC; return ERR;}
			MBVal = MessageBox (0, buf, "Mod Downgrade", MB_ICONEXCLAMATION | MB_YESNO);
			safe_free(buf);
		}
		else if (ver == oldver && FirstInstall == FALSE){
			char *buf = NULL;
			asprintf (&buf, "%s is already installed.", name);
			if(buf == NULL){safe_free(name);
				CURRERROR = errCRIT_MALLOC; return ERR;}
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

//False == ERROR or Mod Collision
//True == No mod collision!
//TODO: Less confusing name? (This checks for presence of requested mods)
BOOL Mod_CheckDep(json_t *root)
{	
	unsigned int i;
	json_t *value, *deps;
	char *ParentUUID = JSON_GetStr(root, "UUID");
	CURRERROR = errNOERR;
	
	if(ParentUUID == NULL){return FALSE;}
	
	deps = json_object_get(root, "dependencies");
	if(!deps || !json_is_array(deps)){
		safe_free(ParentUUID);
		return FALSE;
	}

	json_array_foreach(deps, i, value){
		int ModCount;
		{
			sqlite3_stmt *command;
			const char *query = "SELECT COUNT(*) FROM Mods WHERE UUID = ? AND Ver >= ?";
			char *UUID = JSON_GetStr(root, "UUID");
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
		safe_free(message);
		CURRERROR = errWNG_MODCFG;
		return;
	}
	
	json_array_foreach(deps, i, value){
		int ModCount;
		
		{
			sqlite3_stmt *command;
			const char *query = "SELECT COUNT(*) FROM Mods "
			                          "WHERE UUID = ? AND Ver >= ?";
			char *UUID = JSON_GetStr(root, "UUID");
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
				CURRERROR = errCRIT_MALLOC;
				safe_free(name);
				safe_free(auth);
				return;
			}
			
			while(strlen(message) + strlen(out) + 1 >= messagelen){
				messagelen *= messagelen;
				message = realloc(message, messagelen);
				if(message == NULL){
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
	if(
		strcmp(type, "IEEE32") == 0 ||
		strcmp(type, "IEEE64") == 0
	){
		result = IEEE;
	} else if (
		strcmp(type, "Int8") == 0 ||
		strcmp(type, "Int16") == 0 ||
		strcmp(type, "Int32") == 0
	){
		result = SINT;
	} else if (
		strcmp(type, "uInt8") == 0 ||
		strcmp(type, "uInt16") == 0 ||
		strcmp(type, "uInt32") == 0
	){
		result = USINT;
	} else {
		result = INVALID;
	}
	return result;
}

const char * Var_GetType_Str(enum VarType type){
	switch(type){
	case IEEE: return "IEEE";
	case SINT: return "SINT";
	case USINT: return "USINT";
	default: return "INVALID";
	}
}


struct VarValue Var_GetValue_SQL(const char *VarUUID){
	struct VarValue result;
	json_t *VarArr, *VarObj;
	sqlite3_stmt *command;
	char *query = "SELECT Type, Value FROM Variables WHERE UUID = ?";
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
	case SINT:
		result.sint = JSON_GetInt(VarObj, "Value"); break;
	case USINT:
		result.usint = JSON_GetInt(VarObj, "Value"); break;
	case IEEE:
		result.ieee = JSON_GetDouble(VarObj, "Value"); break;
	default:
		result.type = INVALID; break;
	}
	
	json_decref(VarObj);
	json_decref(VarArr);
	return result;
}

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
	case IEEE:
		result.ieee = 0;
		break;
	case SINT:
		result.sint = 0;
		break;
	case USINT:
	default:
		result.usint = 0;
		break;	
	}
	
	value = JSON_GetStr(VarObj, "Value");
	
	//Check for +/- sign.
	if(value[0] == '+' || value[0] == '-'){
		//Get stored value of variable
		struct VarValue OldVar = Var_GetValue_SQL(JSON_GetStr(VarObj, "UUID"));
		
		switch(OldVar.type){
		case USINT:
			result.usint = OldVar.usint; break;
		case SINT:
			result.sint = OldVar.sint; break;
		case IEEE:
			result.ieee = OldVar.ieee; break;
		default:
			result.usint = 0; break;
		}
	}
	
	//Get value
	switch(result.type){
	case IEEE:
		result.ieee += atof(value); break;
	case SINT:
		result.sint += atoi(value); break;
	case USINT:
	default:
		result.usint += atoi(value); break;	
	}
	
	safe_free(value);
	return result;
}

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

BOOL Var_MakeEntry(json_t *VarObj, const char *ModUUID)
{
	char *UUID = NULL, *PubType = NULL;
	struct VarValue result;
	CURRERROR = errNOERR;
	//VarObj is read from mod's file.
	
	//Get variable value and type
	result = Var_GetValue_JSON(VarObj);
	
	//Get a UUID
	UUID = JSON_GetStr(VarObj, "UUID");

	//Check if variable already exists
	if(Var_GetValue_SQL(UUID).type != INVALID){
		//If so, update existing variable instead of making a new one.
		
		sqlite3_stmt *command;
		const char *query = "UPDATE Variables SET "
		                          "'Value' = ? WHERE UUID = ?;";
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 2, UUID, -1, SQLITE_TRANSIENT)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
		   
		//Swap out Value for appropriate type.
		switch(result.type){
		case SINT:
			sqlite3_bind_int(command, 1, result.sint);
			break;
		case USINT:
			sqlite3_bind_int(command, 1, result.usint);
			break;
		case IEEE:
			sqlite3_bind_double(command, 1, result.ieee);
			break;
		default:
			//Invalid type
			return FALSE;
		}
		
		if(SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
		
		safe_free(UUID);
		return TRUE;
	}
	
	//Get PublicType
	PubType = JSON_GetStr(VarObj, "PublicType");
	
	//If PublicType == list, create and insert the PublicList
	if(strcmp(PubType, "list") == 0){
		json_t *PubList = json_object_get(VarObj, "PublicList");
		Var_CreatePubList(PubList, UUID);
	}
		
		
	//Compose and execute an insert command.
	{	
		sqlite3_stmt *command;
		char *query = "INSERT INTO Variables "
		           "(UUID, Mod, Type, PublicType, Desc, Value) "
			   "VALUES (?, ?, ?, ?, ?, ?);";
		
		//Grab description
		char *Desc = JSON_GetStr(VarObj, "Desc");
		
		//Compose SQL
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 2, ModUUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 3, Var_GetType_Str(result.type), -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 4, PubType, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 5, Desc, -1, SQLITE_TRANSIENT)
		   ) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
				
		//Swap out Value for appropriate type.
		switch(result.type){
		case SINT:
			sqlite3_bind_int(command, 6, result.sint);
			break;
		case USINT:
			sqlite3_bind_int(command, 6, result.usint);
			break;
		case IEEE:
			sqlite3_bind_double(command, 6, result.ieee);
			break;
		default:
			sqlite3_bind_int(command, 6, 0);
		}
		if(SQL_HandleErrors(sqlite3_step(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
		safe_free(Desc);
	}

	safe_free(UUID);
	safe_free(PubType);
	return TRUE;
}

void Var_SetValue(json_t VarObject, int value)
{
	//TODO: This.
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
struct ModSpace Mod_FindSpace(struct ModSpace input, int len, const char *ModUUID, BOOL Clear){
	struct ModSpace result;
	json_t *out, *row;
	char *IDStr = NULL;
	
	//Set defaults for result
	CURRERROR = errNOERR;
	strcpy(result.ID, "");
	result.Start = 0;
	result.End = 0;
	result.FileID = 0;
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
				Clear ? "Clear":"Add", -1, SQLITE_STATIC) |
			sqlite3_bind_int(command, 2, len) |
			sqlite3_bind_int(command, 3, input.Start) |
			sqlite3_bind_int(command, 4, input.End) |
			sqlite3_bind_int(command, 5, input.FileID)
		) != 0){CURRERROR = errCRIT_DBASE; return result;}
		
		out = SQL_GetJSON(command);
		
		if(SQL_HandleErrors(sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return result;
		}
	}
	
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
	strncpy(result.ID, IDStr, MODSPACE_ID_MAX);
	result.ID[MODSPACE_ID_MAX-1] = '\0';
	
	result.FileID = JSON_GetInt(row, "File");
	if(CURRERROR != errNOERR){safe_free(IDStr); return result;}
	result.Start = JSON_GetInt(row, "Start");
	if(CURRERROR != errNOERR){safe_free(IDStr); return result;}
	result.End = JSON_GetInt(row, "End");
	if(CURRERROR != errNOERR){safe_free(IDStr); return result;}
	//Drop if we need to.
	if (ModUUID != NULL){
		//AlertMsg(ModUUID, "Test");
		char *NewID = NULL;
		sqlite3_stmt *command;
		const char *query = "UPDATE Spaces SET UsedBy = ?, ID = ? "
		                    "WHERE ID = ? AND Type = ?";

		//Is this scalable? I hope it is.
		asprintf(&NewID, "%s.old", IDStr);
		
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 ||SQL_HandleErrors(
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 2, NewID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 3, IDStr, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 4,
				Clear ? "Clear":"Add", -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_finalize(command)
		) != 0){
			safe_free(NewID);
			CURRERROR = errCRIT_DBASE;
			return result;
		}
		command = NULL;
		strncpy(result.ID, NewID, MODSPACE_ID_MAX);
		result.ID[MODSPACE_ID_MAX] = '\0';
		safe_free(NewID);
	}
	//safe_free(IDStr);
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
 *  Description:  Creates an clear space marker given a ModSpace struct containing a 
 *                start address, end address and file. Very dumb in operation; make sure
 *                space is indeed ready to be cleared first.
 * =====================================================================================
 */
BOOL ModOp_Clear(struct ModSpace input, const char *ModUUID){
	const int len = input.End - input.Start;
	struct ModSpace Occ;
	char *OccOwner;
	CURRERROR = errNOERR;
	
	//Make sure there isn't anything in the way, first.
	Occ = Mod_FindSpace(input, len, ModUUID, FALSE);
	if(Occ.Valid == TRUE){	//Something found
		OccOwner = NULL;	
	} else {	//Nothing found
		OccOwner = Mod_FindPatchOwner(Occ.ID);
	}
	if(CURRERROR != errNOERR){return FALSE;}
	if(!OccOwner){OccOwner = strdup("");}
	
	if (strcmp(OccOwner, "") != 0){ //If space is occupied
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
		if (strcmp(Occ.ID, "") != 0) {
			ID = strdup(Occ.ID);
		} else {	//Autogen ID from start pos
			asprintf(&ID, "%X", input.Start); 
		}
		
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(
			sqlite3_bind_text(command, 1, ID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_int(command, 2, input.FileID) |
			sqlite3_bind_text(command, 3, ModUUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_int(command, 4, input.Start) |
			sqlite3_bind_int(command, 5, input.End) |
			sqlite3_bind_int(command, 6, len)
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
BOOL Mod_AddSpace(struct ModSpace input, int start, int len, const char *ModUUID){
	//Compute the wanted Start address and End address
	//Tries to go as low as possible, but sometimes the ranges clash.
	int StartAddr = start > input.Start ? start : input.Start;
	int EndAddr = (StartAddr + len) < input.End ? (StartAddr + len) : input.End;
	sqlite3_stmt *command;
	const char *query = "INSERT INTO Spaces "
		"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len') VALUES "
		"(?, ?, ?, ?, ?, ?, ?);";
	CURRERROR = errNOERR;
	
	if(SQL_HandleErrors(
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	
	//If no ID, make one.
	if (strcmp(input.ID, "") == 0){
		char *tempID = NULL;
		asprintf(&tempID, "%X", input.Start);
		strncpy(input.ID, tempID, MODSPACE_ID_MAX);
		input.ID[MODSPACE_ID_MAX] = '\0';
		safe_free(tempID);
	}
	//Create new AddSpc table row
	if(SQL_HandleErrors(
		sqlite3_bind_text(command, 1, input.ID, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 2, "Add", -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 3, input.FileID) |
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_int(command, 5, StartAddr) |
		sqlite3_bind_int(command, 6, EndAddr) |
		sqlite3_bind_int(command, 7, len)
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
BOOL ModOp_Add(struct ModSpace input, int handle, unsigned const char * bytes, int len, const char *ModUUID){
	struct ModSpace FreeSpace;

	//Find some free space to put it in
	FreeSpace = Mod_FindSpace(input, len, ModUUID, TRUE);
	
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
	strncpy(FreeSpace.ID, input.ID, MODSPACE_ID_MAX);
	FreeSpace.ID[MODSPACE_ID_MAX] = '\0';
	
	//Make an add space
	/*if(Mod_AddSpace(FreeSpace, input.Start, len, ModUUID) == FALSE){
		return FALSE;
	}*/
	
	//Read existing bytes into revert table and write new bytes in
	{
		unsigned char *OldBytesRaw = NULL;
		char *OldBytes = NULL;
		sqlite3_stmt *command;
		const char *query = "INSERT INTO Revert "
			"('PatchUUID', 'OldBytes') VALUES (?, ?);";
		
		//Read the old bytes
		OldBytesRaw = calloc(len, sizeof(char));
		//Quick error checking
		if(OldBytesRaw == NULL){
			CURRERROR = errCRIT_MALLOC;
			return FALSE;
		}
		
		if (read(handle, OldBytesRaw, len) != len){
			AlertMsg("Unexpected amount of bytes read in ModOp_Add.\n"
				"Please report this to mod loader developer.",
				"Mod Loader error");
			safe_free(OldBytesRaw);
			CURRERROR = errCRIT_FUNCT;
			return FALSE;
		}
		
		//Convert to hex string
		OldBytes = Bytes2Hex(OldBytesRaw, len);
		safe_free(OldBytesRaw);
		
		//Construct SQL Statement
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

unsigned char * Mod_GetBytes(json_t *patchCurr, int *len)
{
	char *ByteStr = NULL, *ByteMode = NULL;
	unsigned char *bytes = NULL;
	
	//Get Byte Mode and String.
	ByteStr = JSON_GetStr(patchCurr, "Value");
	ByteMode = JSON_GetStr(patchCurr, "AddType");
	
	//Error checking
	if (strcmp(ByteMode, "") == 0){
		char *Mode;
		Mode = JSON_GetStr(patchCurr, "Mode");
		
		//If there's not supposed to be one, that's fine
		if (strcmp(Mode, "Clear") == 0 || strcmp(Mode, "Copy") == 0 || strcmp(Mode, "Move") == 0 ){
			//Re-define ByteStr and ByteMode
			safe_free(ByteStr);
			safe_free(ByteMode);
			ByteStr = strdup("");
			ByteMode = strdup("Bytes");
			
			safe_free(Mode);
		} else {
			safe_free(Mode);
			return NULL;
		}
	}
	
	///Possible byte modes:
	//  "Bytes"      	- raw data in hex form to be copied exactly. Use for very short snippets of data or code.
	//  "Path"        	- path to file inside mod's folder. Use for resources or large code samples.
	//  "UUIDPointer" 	- Give it the GUID of a space and it will give the allocated memory address. For subroutines, etc.
	//  "VarValue"   	- Give it the GUID of a mod loader variable and it will give it's contents.
	if (strcmp(ByteMode, "Bytes") == 0){
		//Get the bytes to insert
		bytes = Hex2Bytes(ByteStr, len);
	}
	
	/*if (strcmp(ByteMode, "UUIDPointer") == 0){
		//Find address for ID
		char command[256];
		char *out = NULL;
		sprintf(command, "SELECT Start FROM Spaces WHERE ID = '%s';", ByteStr);
		sqlite3_exec(CURRDB, command, SQLCB_RetString, out, NULL);
		
		//Make that address the bytes
		len = (strlen(ByteStr)-2)/2;
		bytes = Hex2Bytes(out, len);
		
		safe_free(out);
	}*/
	safe_free(ByteStr);
	safe_free(ByteMode);
	return bytes;
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
		int file, len;
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
			retval = FALSE;
			goto Mod_Install_Cleanup;
		}
		
		//Get Patch Mode
		Mode = JSON_GetStr(patchCurr, "Mode");
		if(Mode == NULL){
			CURRERROR = errCRIT_MALLOC;
			retval = FALSE;
			goto Mod_Install_Cleanup;
		}
		
		//Get bytes and error-handle it
		bytes = Mod_GetBytes(patchCurr, &len);
		if(
			bytes == NULL &&
			!(strcmp(Mode, "Clear") == 0 ||
			strcmp(Mode, "Copy") == 0 ||
			strcmp(Mode, "Move") == 0 )
		){
			char *errormsg = NULL;
			asprintf(&errormsg,
				"%sThe 'Value' parameter is missing on patch no. %i.",
				errormsg_ModInst_CantCont, i
			);
				   
			AlertMsg(errormsg, "Mod Metadata Error");
			safe_free(errormsg);

			retval = FALSE;
			goto Mod_Install_Cleanup;
		}
		
		input.Start = JSON_GetInt(patchCurr, "Start");
		input.End = JSON_GetInt(patchCurr, "End") + 1;
		input.FileID = File_GetID(JSON_GetStr(patchCurr, "File"));
		if(CURRERROR != errNOERR){
			retval = FALSE;
			goto Mod_Install_Cleanup;
		}
		//We might write to input.ID, so clear it out for later.
		strcpy(input.ID, "");
		 
		//Check if Start is a UUID
		if (!json_is_integer(json_object_get(patchCurr, "Start"))){
			char *UUID = JSON_GetStr(patchCurr, "Start");
			//Replace Start and End with UUID's start/end
			Mod_FindUUIDLoc(&input.Start, &input.End, UUID);
			
			//If we're doing a replacement, make the new patch
			//take the UUID of the old space.
			if(strcmp(Mode, "Repl") == 0){
				strncpy(input.ID, UUID, MODSPACE_ID_MAX);
				input.ID[MODSPACE_ID_MAX] = '\0';
			}
		}
		
		//Set UUID if unset
		if(strcmp(input.ID, "") == 0){
			char *tempUUID = JSON_GetStr(patchCurr, "UUID");
			
			//If there was no UUID, make one up. (Hex representation of start)
			if(strcmp(tempUUID, "") == 0){
				free(tempUUID);
				asprintf(&tempUUID, "%X", input.Start);
			}
			
			strncpy(input.ID, tempUUID, MODSPACE_ID_MAX);
			input.ID[MODSPACE_ID_MAX] = '\0';
			free(tempUUID);
		}
		
		///Parse commands (finally)
		if(strcmp(Mode, "Repl") == 0){
			if (ModOp_Clear(input, ModUUID) == FALSE){
				close(file);
				retval = FALSE;
				if(CURRERROR != errNOERR){
					goto Mod_Install_Cleanup;
				}
				break;
			}
			if (ModOp_Add(input, file, bytes, len, ModUUID) == FALSE){
				close(file);
				retval = FALSE;
				if(CURRERROR != errNOERR){
					goto Mod_Install_Cleanup;
				}
				break;
			}
		} else if(strcmp(Mode, "Clear") == 0){
			if (ModOp_Clear(input, ModUUID) == FALSE){
				close(file);
				retval = FALSE;
				if(CURRERROR != errNOERR){
					goto Mod_Install_Cleanup;
				}
				break;
			}
		} else if(strcmp(Mode, "Add") == 0){
			if (ModOp_Add(input, file, bytes, len, ModUUID) == FALSE){
				close(file);
				retval = FALSE;
				if(CURRERROR != errNOERR){
					goto Mod_Install_Cleanup;
				}
				break;
			}
		}
		//Will add Copy and Move later. Low-priority.
		//Move is higher-priority because that's awesome for finding space
		else{
			AlertMsg("Unknown patch mode", Mode);
			retval = FALSE;
			break;
		}
		
		close(file);
		ProgDialog_Update(ProgDialog, 1);
	}
Mod_Install_Cleanup:
	Mod_AddToDB(root);
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
		) != 0){return FALSE;}
		
		if(SQL_HandleErrors(sqlite3_step(command)
		) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
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
	
	//Get stopping point
	if(strcmp(ModUUID, "") != 0){
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
	} else {
		//Database is corrupted. Work around it.
		AlertMsg("The mod database appears to be corrupted.\n"
			 "You may need to delete the mod database and\n"
			 "reinstall your game's files if the mod loader\n"
			 "begins to behave erratically.\n",
			 "Database Corruption Error");
		modCount = 0; modStop = 0;
		return FALSE;
	}
	
	//Init progress box
	ProgDialog = ProgDialog_Init(modCount, "Uninstalling Mod...");
	
	//Perform operations in reverse
	for (i = modCount; i >= modStop; i--){

		//Get list of operations
		json_t *out, *row;
		unsigned int j;
		sqlite3_stmt *command;
		
		{
			const char *query = "SELECT Spaces.* FROM Spaces "
				"JOIN Mods ON Spaces.Mod = Mods.UUID "
				"WHERE Mods.RowID = ?;";
			
			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(sqlite3_bind_int(command, 1, i)
			) != 0){CURRERROR = errCRIT_DBASE;
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
			
			out = SQL_GetJSON(command);
			sqlite3_finalize(command); command = NULL;
		}
		
		//Loop through operations
		json_array_foreach(out, j, row){
			//Find type
			char *SpaceType = JSON_GetStr(row, "Type");
			if(strcmp(SpaceType, "Add") == 0){
				if(ModOp_UnAdd(row) == FALSE){
					retval = FALSE;
					goto Mod_Uninstall_Cleanup;
				};
			} else if (strcmp(SpaceType, "Clear") == 0) {
				if(ModOp_UnClear(row) == FALSE){
					retval = FALSE;
					goto Mod_Uninstall_Cleanup;
				};
			}
			safe_free(SpaceType);
		}
		json_decref(out);
		
		//Mod has now been uninstalled.
		//Unmark used spaces
		{
			const char *query = "UPDATE Spaces SET UsedBy = NULL WHERE "
			             "UsedBy = (SELECT UUID FROM Mods WHERE RowID = ?);";
			if(SQL_HandleErrors(
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(sqlite3_bind_int(command, 1, i)
			) != 0 || SQL_HandleErrors(sqlite3_step(command)
			) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
				CURRERROR = errCRIT_DBASE;
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
			command = NULL;
		}
		
		
		//Remove mod entry
		{
			const char *query = "DELETE FROM Mods WHERE RowID = ?;";
			if(SQL_HandleErrors(sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(sqlite3_bind_int(command, 1, i)
			) != 0 || SQL_HandleErrors(sqlite3_step(command)
			) != 0 || SQL_HandleErrors(sqlite3_finalize(command)) != 0){
				CURRERROR = errCRIT_DBASE;
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
			command = NULL;
		}
		
		//Increment progress bar
		ProgDialog_Update(ProgDialog, 1);
	}
	
Mod_Uninstall_Cleanup:
	//Kill progress box
	ProgDialog_Kill(ProgDialog);
	
	//Reinstall existing mods
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
	char *ModName;
	char *ModUUID;
	int len, CurrSel;
	HWND list;
	
	//Get the handle for the mod list
	list = GetDlgItem(hwnd, IDC_MAINMODLIST);
	
	//Get the current selection
	CurrSel = SendMessage(list, LB_GETCURSEL, 0, 0);
	if(CurrSel == -1){return FALSE;} //No error -- nothing selected
	
	//Get the length of the name of the selected mod
	len = SendMessage(list, LB_GETTEXTLEN, CurrSel, 0);
	
	//Allocate space for the name
	ModName = malloc(len+1);
	if(ModName == NULL){CURRERROR = errCRIT_MALLOC; return FALSE;}
	
	//Actually get the name
	SendMessage(list, LB_GETTEXT, CurrSel, (LPARAM)ModName);
	
	//Use that to get the UUID
	{
		sqlite3_stmt *command = NULL;
		char *query = "SELECT UUID FROM Mods WHERE Name = ?;";
		if(SQL_HandleErrors(
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
		if(SQL_HandleErrors(
			sqlite3_bind_text(command, 1, ModName, -1, SQLITE_TRANSIENT)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
		ModUUID = SQL_GetStr(command);
		if(ModUUID == NULL){return FALSE;}
		sqlite3_finalize(command); command = NULL;
	}	
	safe_free(ModName);
	
	//Finally remove the mod
	EnableWholeWindow(hwnd, FALSE);
	//sqlite3_exec(CURRDB, "BEGIN TRANSACTION", NULL, NULL, NULL);
	Mod_Uninstall(ModUUID);
	ErrCracker(CURRERROR);
		//AlertMsg("Uninstallation failure. Rolling back...", "Uninstallation Failure");
		///Again, same issue. Bleh. Just... ignore it and deal with the fallout.
		//sqlite3_exec(CURRDB, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
	//sqlite3_exec(CURRDB, "COMMIT TRANSACTION", NULL, NULL, NULL);
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
		sqlite3_open(":memory:", &CURRDB)
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
		char *tempDir = NULL;
		BOOL canQuit = FALSE;
		
		// Load program preferences/locations
		json_t *GameCfg = JSON_Load(GAMECONFIG);
		assert(GameCfg != NULL);
		
		//Set program icon
		SendMessage(hwnd, WM_SETICON, ICON_SMALL,
			(LPARAM)LoadImage(GetModuleHandle(NULL),
			MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0));
		SendMessage(hwnd, WM_SETICON, ICON_BIG,
			(LPARAM)LoadIcon(GetModuleHandle(NULL),
			MAKEINTRESOURCE(IDI_ICON)));
		
		
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
				safe_free(tempDir);
				//Fix config file?
				return TRUE;
				
			case errCRIT_FILESYS:
				AlertMsg(errormsg_Crit_Sys, "File System Failure!");
				//fall-through
			case errUSR_ABORT:
				CURRERROR = errUSR_QUIT;
				//fall-through
			case errUSR_QUIT:
				safe_free(tempDir);
				return TRUE;
				
			default:
				break;
			}
			//Other errors
			ErrCracker(CURRERROR);
		} while (canQuit == FALSE);
		CURRERROR = errNOERR;
		CURRDIR = tempDir;
		//DO NOT free tempDir
		
		SQL_Load();
		ErrCracker(CURRERROR);
		SQL_Populate(GameCfg);
		ErrCracker(CURRERROR);
		
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
			DialogBox(GetModuleHandle(NULL),
				MAKEINTRESOURCE(IDD_ABOUT), hwnd, Dlg_Generic);
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
	
	/*#if (_WIN32_IE < 0x0300)
		InitCommonControls();
	#else
	{
		const INITCOMMONCONTROLSEX controls;
		controls.dwICC = ICC_WIN95_CLASSES;
		InitCommonControlsEx(controls);
	}
	#endif
*/
	hDialog = CreateDialog (hInst, MAKEINTRESOURCE (IDD_MAIN), 0, Dlg_Main);
	//json_set_alloc_funcs(malloc, free);

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

