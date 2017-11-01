/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//Includes and macros
#include <stdlib.h>                    // Malloc and other useful things
#include <errno.h>                     // Catching and explaining errors
#include <stdarg.h>                    // Indeterminate argument support
#include <io.h>                        // Standardized I/O library. (Portable!)
#include <fcntl.h>                     // Flags for open() (O_RDWR, O_RDONLY)
#include <dir.h>                       // Switch and manipulate directories
#include <assert.h>                    // Debugging
#include <math.h>                      // HUGE_VAL

#include <windows.h>                   // Win32 APIs for the frontend
#include <commctrl.h>                  // Common Controls for listbox, etc.
#include <Shlobj.h>                    // "Open Folder" dialog 

#include <jansson.h>                   // EXTERNAL (http://www.digip.org/jansson/)
                                       // JSON parser. for loading patches, etc.
				       // and parsing SQL results
#include <sqlite3.h>                   // EXTERNAL (http://sqlite.org)
                                       // Embedded database. Used for quick lookups
				       // of existing mods, dependencies, variables, etc.				      

#include "resource.h"                  // LOCAL: Defines IDs so we can access resources
#include "funcproto.h"                 // LOCAL: Function prototypes and structs
				    
#define PROGVERSION_MAJOR 1            // Major version. Compatibility-breaking.
#define PROGVERSION_MINOR 0            // Minor version. Shiny new features.
#define PROGVERSION_BUGFIX 0           // Bugfix release. For bugfixes and stuff.
const long PROGVERSION =               // Program version in format 0x00MMmmbb
	PROGVERSION_BUGFIX + 
	(PROGVERSION_MINOR*0x100) +
	(PROGVERSION_MAJOR*0x10000);
const char PROGAUTHOR[] = "InvisibleUp"; // Name of program author (don't change please.)
const char PROGSITE[] = "http://github.com/InvisibleUp/SrModLdr/"; // Site to get new revisions (don't change please.) 

char CURRDIR[MAX_PATH] = ""; //Current root directory of game
sqlite3 *CURRDB; //Current database holding patches 

//Temporary until we get a config file going
const char GAMECONFIG[] = "./games/sonicr.json";	

///Win32 helper functions
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  AlertMsg
 *  Description:  Wrapper for Win32 MessageBox.
 *                The point here is cross-platform/UI compatibility.
 * =====================================================================================
 */
void AlertMsg (char *Message, char *Title)
{
	MessageBox(0, Message, Title, MB_ICONEXCLAMATION|MB_OK);
	return;
}		/* -----  end of function AlertMsg  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  EnableWholeWindow
 *  Description:  Enables or disables all elements in a window and repaints it.
 *                Userful for preventing user actions during installation or work
 * =====================================================================================
 */
void EnableWholeWindow(HWND hwnd, BOOL state)
{
	HWND hCtl = GetWindow(hwnd,GW_CHILD);
	//Disable all controls, but not main window.
	//This prevents window from flying into BG.
	while (hCtl) {
		EnableWindow(hCtl, state);
		//UpdateWindow(hCtl);
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
const char * SelectFolder(LPCSTR title)
{
	BROWSEINFO bi = { 0 }; // For init directory selector
	LPITEMIDLIST pidl;
	int errorcode;
	char path[MAX_PATH] = "";
	
	bi.lpszTitle = title;
	
	do {
		errorcode = 0;
		pidl = SHBrowseForFolder ( &bi );
		// If user hits Cancel, stop nagging user
		if (pidl == 0){ 
			return "";
		}
		
		// Get the name of the folder
		if (!SHGetPathFromIDList ( pidl, path )){
			// Force user to pick again if not an usable directory
			//("My Computer", "Recycle Bin", etc.)
			errorcode = 1;
		}
		
		//User chose bad folder
		if (errorcode != 0) {
			AlertMsg(
				"There was an error opening the folder you selected.\n"
				"Make sure it actually is a folder you can use and not\n"
				"'My Computer' or another system folder.",
				"Not a Folder"
			);
		}
	} while (errorcode != 0);
	return path;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SelectFile
 *  Description:  Simple wrapper around GetOpenFileName.
 * =====================================================================================
 */
void SelectFile(HWND hwnd, char *fpath, char *fname, char *Filter)
{
	OPENFILENAME ofn = {0};
	//char fname[MAX_PATH] = "";

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
	
	//Get just the filename
	strcpy(fname, fpath+ofn.nFileOffset);
}

HWND ProgDialog_Init(int max, char *label)
{
	HWND ProgDialog;
	//Create progress window in modeless mode
	ProgDialog = CreateDialog(
		GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDD_PROGRESS),
		0,
		Dlg_Generic
	);
	assert(ProgDialog != NULL);
	
	//Set progress bar maximum
	SendMessage(
		GetDlgItem(ProgDialog, IDC_PROGBAR),
		PBM_SETRANGE,
		0,
		MAKELPARAM(0, max)
	);
	
	//Set text label
	SetWindowText(ProgDialog, label);
	
	return ProgDialog;
}

void ProgDialog_Update(HWND ProgDialog, int Delta)
{
	HWND ProgBar;
	
	//Get progress bar control.
	ProgBar = GetDlgItem(ProgDialog, IDC_PROGBAR);
	
	//Update bar
	SendMessage(ProgBar, PBM_DELTAPOS, Delta, 0);
	//Weird Aero hack so you can actually see progress.
	SendMessage(ProgBar, PBM_DELTAPOS, -1, 0);
	SendMessage(ProgBar, PBM_DELTAPOS, 1, 0);
	
	return;
}

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
void UpdateModList(HWND list)
{
	json_t *out, *row;
	//int i, LastItem = -1;
	unsigned int i;

	//sqlite3_exec(CURRDB, "SELECT Name FROM Mods", SQLCB_RetJSON, NULL, NULL);
	out = SQL_GetJSON("SELECT Name FROM Mods");
	//7 strings per mod. Separated by \v.
	if(out == NULL){return;}
	//Count number of mods.
	//modCount = CountChar(out, '\v');
	
	//Clear list
	SendMessage(list, LB_RESETCONTENT, 0, 0);
	
	//Assemble and write out the names
	json_array_foreach(out, i, row){
		char *name = NULL;
		//Phantom entries make this angry.
		if(JSON_GetStrLen(row, "Name") == 0){ continue;}
		name = JSON_GetStr(row, "Name");
		//LastItem = SendMessage(list, LB_ADDSTRING, 0, (LPARAM)name);
		SendMessage(list, LB_ADDSTRING, 0, (LPARAM)name);
		free(name);
	}
	//Auto-select the last item in the list
	//SendMessage(list, LB_SETCURSEL, LastItem, 0);
	json_decref(out);
}

void UpdateModDetails(HWND hwnd, int listID)
{
	char *command = NULL;
	json_t *out, *row;
	char *name = NULL, *auth = NULL, *cat = NULL, *desc = NULL, *ver = NULL;
	
	//Select the right mod
	asprintf(&command, "SELECT * FROM Mods WHERE RowID = %d;", listID+1);
	if(command == NULL){return;}
	
	out = SQL_GetJSON(command);
	if (out == NULL){return;}
	if(!json_is_array(out)){return;}
	
	row = json_array_get(out, 0);
	if (row == NULL){return;}
	if(!json_is_object(row)){return;}
	
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
	
	free(auth); free(cat); free(desc); free(ver); free(command);
	
	//Get used EXE space
	if(strcmp(name, "") != 0){
		char *EXESpc = NULL;
		char *UUID = JSON_GetStr(row, "UUID");
		asprintf(&EXESpc, "%d bytes", GetUsedSpaceNum(UUID, 1));
		
		//Write to dialog
		SetWindowText(GetDlgItem(hwnd, IDC_MAINEXESPACE), EXESpc);
		free(EXESpc);
	} else {
		SetWindowText(GetDlgItem(hwnd, IDC_MAINEXESPACE), "");
	}
	free(name);
	json_decref(out);
	return;
}

int GetUsedSpaceNum(char *ModUUID, int File)
{
	int add, clear;
	char *command = NULL;
	
	asprintf(&command,
		"SELECT SUM(Len) FROM Spaces WHERE "
		"File = %i AND Mod = '%s' AND Type = 'Add';",
		File, ModUUID
	);
	add = SQL_GetNum(command);
	free(command);
	
	asprintf(&command,
		"SELECT SUM(Len) FROM Spaces WHERE "
		"File = %i AND Mod = '%s' AND Type = 'Clear';",
		File, ModUUID, ModUUID
	);
	clear = SQL_GetNum(command);
	free(command);

	return (add - clear);
}

///SQLite helper Functions

//I'm sorry, I'm not typing all this out every time.
char * SQL_ColName(sqlite3_stmt * stmt, int col){
	return sqlite3_column_name(stmt, col) == NULL ?
		"" : sqlite3_column_name(stmt, col);
}

char * SQL_ColText(sqlite3_stmt * stmt, int col){	
	return sqlite3_column_text(stmt, col) == NULL ?
		"" : sqlite3_column_text(stmt, col);
}

/*json_t * SQL_GetJSON(char *query)
{
	unsigned int len = 256; //Safe initial amount
	char *JSONStr = malloc(len);
	sqlite3_stmt *stmt;
	int i, errorNo;
	json_t *result;
	json_error_t error;
	
	///Prepare the SQL statement
	sqlite3_prepare_v2(CURRDB, query, strlen(query)+1, &stmt, NULL);
	
	///Compose the JSON string
	strcpy(JSONStr, "["); //It's an array, regardless
	
	errorNo = sqlite3_step (stmt);
        do {
		//Never can be too safe, you know
		while(strlen(JSONStr) + 1 >= len){
			len *= len; JSONStr = realloc(JSONStr, len);
		}
		strcat(JSONStr, "{ ");
		
		for(i = sqlite3_column_count(stmt) - 1; i >= 0; i--){
			char *temp = NULL;
			int templen;
			char *colname = strdup(SQL_ColName(stmt, i));
			char *coltext = strdup(SQL_ColText(stmt, i));
			
			if(strcmp(coltext, "") == 0){
				templen = asprintf(&temp, "\"%s\":null,", colname);
			} else {
				templen = asprintf(&temp, "\"%s\":\"%s\",", colname, coltext);
			}
			
			free(colname);
			free(coltext);
			
			while(templen + strlen(JSONStr) >= len){
				len *= len; JSONStr = realloc(JSONStr, len);
			}
			
			strcat(JSONStr, temp);
		}
		
		while(strlen(JSONStr) + 1 >= len){
			len *= len; JSONStr = realloc(JSONStr, len);
		}
		JSONStr[strlen(JSONStr)-1] = '}';
		strcat(JSONStr, ",");
		
		errorNo = sqlite3_step (stmt);
        } while (errorNo == SQLITE_ROW);
	JSONStr[strlen(JSONStr)-1] = ']';
	sqlite3_finalize(stmt);
	
	///Make it a JSON object
	//AlertMsg(JSONStr, "JSON String");
	result = json_loads(JSONStr, 0, &error);
	free(JSONStr);

	if(!result){
		char *errMsg = NULL;
		asprintf(&errMsg, "line %d: %s\nContact mod loader dev.",
			error.line, error.text
		);
		AlertMsg(errMsg, "SQL->JSON Error");
		free(errMsg);
	}
	
	return result;
}*/

json_t * SQL_GetJSON(char *query)
{
	//unsigned int len = 256; //Safe initial amount
	//char *JSONStr = malloc(len);
	sqlite3_stmt *stmt;
	int i, errorNo;
	json_t *result = json_array();
	
	///Prepare the SQL statement
	sqlite3_prepare_v2(CURRDB, query, strlen(query)+1, &stmt, NULL);
	
	///Compose the JSON array
	errorNo = sqlite3_step (stmt);
        do {
		//For each row of results, create a new objetc
		json_t *tempobj = json_object();
		
		for(i = sqlite3_column_count(stmt) - 1; i >= 0; i--){
			//For each result key:value pair, insert into tempobj
			//key "colname" with value "coltext".
			json_t *value;
			char *colname = strdup(SQL_ColName(stmt, i));
			char *coltext = strdup(SQL_ColText(stmt, i));
			
			if(strcmp(coltext, "") == 0){
				value = json_null();
			} else {
				value = json_string(coltext);
			}
			 
			json_object_set(tempobj, colname, value);
			json_decref(value);
			
			free(colname);
			free(coltext);
		}
		//Add object to array
		json_array_append_new(result, tempobj);
		
		errorNo = sqlite3_step (stmt);
        } while (errorNo == SQLITE_ROW);
	//End array
	sqlite3_finalize(stmt);
	
	return result;
}

int SQL_GetNum(char *query)
{
	sqlite3_stmt *stmt;
	int errorNo, result = 0;
	
	sqlite3_prepare_v2(CURRDB, query, strlen(query)+1, &stmt, NULL);

        errorNo = sqlite3_step (stmt);
        if (errorNo == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);
        }
	
	sqlite3_finalize(stmt);
	return result;
}

char * SQL_GetStr(char *query)
{
	sqlite3_stmt *stmt;
	int errorNo;
	char *result;
	
	sqlite3_prepare_v2(CURRDB, query, strlen(query)+1, &stmt, NULL);

        errorNo = sqlite3_step (stmt);
        if (errorNo == SQLITE_ROW) {
		result = strdup(SQL_ColText(stmt, 0));
        }
	else {
		result = strdup("");
	}
	
	sqlite3_finalize(stmt);
	return result;
}

///Jansson helper functions

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_Load
 *  Description:  Safely gives a json_t pointer to a JSON file
 *                given a filename. Only works with object roots.
 * =====================================================================================
 */

json_t * JSON_Load(char *fpath)
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
		free(buf);
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
size_t JSON_GetInt(json_t *root, char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL) {return 0;}
	if (json_is_string(object)) {
		return atoi(JSON_GetStr(root, name));
	}
	return json_integer_value(object);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_GetInt
 *  Description:  Gives a integer given a JSON tree and the name of a tag
 * =====================================================================================
 */
double JSON_GetDouble(json_t *root, char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL) {return 0;}
	if (json_is_string(object)) {
		return atof(JSON_GetStr(root, name));
	}
	return json_number_value(object);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_GetStr
 *  Description:  Gives a string given a JSON tree and the name of a tag
 * =====================================================================================
 */
char * JSON_GetStr(json_t *root, char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL) {
		return strdup("");
	}
	if (json_is_integer(object)) {
		//WARNING: Possible memory leak if used improperly
		int outint = JSON_GetInt(root, name);
		char *out = NULL;
		asprintf(&out, "%d", outint);
		return out;
	} else if (json_is_real(object)) {
		//WARNING: Possible memory leak if used improperly
		int outint = JSON_GetDouble(root, name);
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
int JSON_GetStrLen(json_t *root, char *name)
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
BOOL File_Exists(char file[MAX_PATH], BOOL InFolder, BOOL ReadOnly)
{
	int mode = ReadOnly ? (F_OK) : (F_OK | W_OK);
	
	char objName[8]; //Proper dialog text
	if (InFolder){ strcpy(objName, "Folder"); }
	else { strcpy(objName, "Folder"); }
	
	if (access(file, mode) != 0){
		switch(errno){
		case ENOENT:{ // Not found error
			char *buf;
			if(InFolder){
				asprintf (&buf,
					"This folder does not contain %s.\n"
					"Please choose one that contains '%s'\n"
					"and all other required files.", file, file);
			}
			else{
				asprintf (&buf,
					"The file %s could not be found.\n"
					"Please choose a different file.", file);
			}
			AlertMsg (buf, "File not Found");
			free(buf);
			return FALSE;
		}
		case EACCES:{ // Read only error
			char *buf;
			asprintf (&buf,
			"This %s is read-only. Make sure this %s is\n"
			"on your hard disk and not on a CD, as this program\n"
			"cannot work directly on a CD.", objName, objName);
			AlertMsg (buf, "Read-Only");
			free(buf);
			return FALSE;
		}
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
BOOL File_Known(
	char *FileName,
	char *GameName,
	int handle,
	json_t *whitelist
){
	size_t fsize, fchecksum;
	size_t i;
	json_t *value;
	BOOL SizeOK = FALSE, CRCOK = FALSE;
	
	//Get filesize
	fsize = filelength(handle);
	//Get the checksum of the file
	fchecksum = crc32(handle, fsize);
	json_array_foreach(whitelist, i, value){
		char *temp = JSON_GetStr(value, "ChkSum");
		size_t ChkSum = strtol(temp, NULL, 16);
		size_t Size = JSON_GetInt(value, "Size");
		free(temp);
		
		if (fsize == Size){ SizeOK = TRUE; }
		if (fchecksum == ChkSum){ CRCOK = TRUE; }
	}
		
	//Return if the file is unknown
	if (SizeOK == FALSE || CRCOK == FALSE){
		char *buf = NULL;
		asprintf (&buf, "The %s file does not match any known version.\n"
			"This means you have an incompatible version of %s.\n"
			"Make sure you have the right revision!",
			FileName, GameName
		);
		AlertMsg (buf, "Incompatible File");
		free(buf);
		return FALSE;
	}
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_OpenSafe
 *  Description:  Safely opens a file by warning the user if the operation fails.
 *                Simple wrapper around io.h's _open function.
 * =====================================================================================
 */
int File_OpenSafe(char filename[MAX_PATH], int flags)
{
	int handle;
	handle = _open(filename, flags);
	//Make sure file is indeed open. (No reason it shouldn't be.)
	if (handle == -1){
		char *buf = NULL;
		asprintf(&buf,
			"An unknown error occurred when opening %s.\n"
			"Make sure your system is not low on resources and try again.\n",
			filename);
		assert(buf != NULL); //Panic. Heavily.
		AlertMsg (buf, "General I/O Error");
		free(buf);
	}
	return handle;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_WriteBytes
 *  Description:  Given an offset and a byte arry of data write it to the given file.
 * =====================================================================================
 */
void File_WriteBytes(int filehandle, int offset, unsigned char *data, int datalen)
{
	lseek(filehandle, offset, SEEK_SET);
	write(filehandle, data, datalen);
	return;
}

// Itoa that outputs the length of whatever the number is
int ItoaLen(size_t input){
	return snprintf(NULL, 0, "%d", input);
}

int FtoaLen(double input){
	return snprintf(NULL, 0, "%f", input);
}

// Finds the maximum value of a list of arguments
int MultiMax(unsigned int count, ...)
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
unsigned char * Hex2Bytes(const char *hexstring, int len)
{
	const char *pos = hexstring;
	unsigned char * val;
	int i = 0;
	val = malloc(len);
	/* WARNING: no sanitization or error-checking whatsoever */
	for(i = 0; i <= len; i++) {
		sscanf(pos, "%2hhx", &val[i]);
		pos += 2;
	}
	return val;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Bytes2Hex
 *  Description:  Converts an array of bytes to a hexadecimal string.
 * =====================================================================================
 */
char * Bytes2Hex(unsigned char *bytes, int len)
{
	char *val;
	int i = 0;

	val = calloc(len*2+2, sizeof(char));
	for(i = 0; i <= len; i++) {
		char temp[4] = "";
		sprintf(temp, "%02X", bytes[i]);
		strcat(val, temp);
	}
	//AlertMsg(val, "Loop Done!");
	return val;
}

///Win32-specific IO functions

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SelectInstallFolder
 *  Description:  Picks a directory and tests if the contained files are valid according
 *                to a given configuration file.
 *   Ret Values:  -1 == User wants to quit.
 *                 0 == Directory picked.
 *                 1 == Invalid directory.
 * =====================================================================================
 */
int SelectInstallFolder(json_t *GameCfg)
{
	int handle;
	char *path;
	char *title;
	char *GameName;
	char *GameEXE;
	
	GameName = JSON_GetStr(GameCfg, "GameName");
	GameEXE = JSON_GetStr(GameCfg, "GameEXE");
	title = malloc(strlen(GameName) + strlen("Pick  Installation directory."));
	
	strcpy (title, "Pick ");
	strcat (title, GameName);
	strcat (title, " Installation directory.");
	
	path = strdup(SelectFolder(title)); //Pick directory
	free(title);
		
	// User refuses to give a directory
	if (strcmp(path, "") == 0) {
		return -1;
	}
	
	strcpy(CURRDIR, path); // Set directory as current directory
	chdir(CURRDIR); // Move to the directory chosen.
	free(path);
	
	// Make sure it has the game file
	if(!File_Exists(GameEXE, TRUE, TRUE)){
		return 1;
	}
	
	//Now we open the file so we can read it.
	handle = File_OpenSafe(GameEXE, _O_BINARY|_O_RDWR);
	if (handle == -1){
		free(GameName);
		free(GameEXE);
		return 1;
	};
	
	//Check if file is known
	{
		BOOL result = File_Known(
			GameEXE,
			GameName,
			handle,
			json_object_get(GameCfg, "Whitelist")
		);
		if(result == FALSE){return 1;}
	}
	
	//Close handle (this cannot fail)
	close(handle); 
	free(GameName);
	free(GameEXE);
	
	return 0; //The path is good! Return!
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
	if (!json_is_string(json_object_get(root, "ML_Ver"))){	
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
	free(ML_Str);
	
	//Make sure version string is formatted properly
	if (error_no != 3){
		AlertMsg (	"This mod is trying to ask for an invalid mod loader version.\n\n"
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
		free(buf);
		return FALSE;
	}
	return TRUE;
}

int Mod_CheckConflict(json_t *root)
{
	// Get UUID and name
	int oldver;
	
	// Check for conflicts
	{
		char *command = NULL;
		char *UUID = JSON_GetStr(root, "UUID");
		asprintf(&command, "SELECT Ver FROM Mods WHERE UUID = '%s';", UUID);
		if(command == NULL){return -1;}
		oldver = SQL_GetNum(command);
		free(UUID);
		free(command);
	}
	if (oldver != 0){
		char *name = JSON_GetStr(root, "Name");
		int MBVal, ver; // Message box response, version req.
		//Get name
		
		//Check version number
		ver = JSON_GetInt(root, "Ver");
		if(ver > oldver){
			char *buf = NULL;
			asprintf (&buf, 
				"The mod you have selected is a newer version of\n"
				"'%s', which you have already installed.\n"
				"Do you want to upgrade from version %d to version %d?",
				name, oldver, ver);
			if(buf == NULL){return -1;}
			MBVal = MessageBox (0, buf, "Mod Upgrade", MB_ICONEXCLAMATION | MB_YESNO);
			free(buf);
		}
		else if (ver < oldver){
			char *buf = NULL;
			asprintf (&buf, 
				"The mod you have selected is an OLDER version of\n"
				"'%s', which you have already installed.\n"
				"Do you want to downgrade from version %d to version %d?",
				name, oldver, ver);
			if(buf == NULL){return -1;}
			MBVal = MessageBox (0, buf, "Mod Downgrade", MB_ICONEXCLAMATION | MB_YESNO);
			free(buf);
		}
		else{
			char *buf = NULL;
			asprintf (&buf, "%s is already installed.", name);
			if(buf == NULL){return -1;}
			AlertMsg (buf, "Mod Already Installed");
			MBVal = IDNO;
			free(buf);
		}
		
		free(name);
		if (MBVal == IDNO){return 2;} //Do not replace
		else{return 1;} //Do replace
	}
	
	return 0; //No conflict
}

//TODO: Switch return values
//TODO: Less confusing name? (This checks for presence of requested mods)
BOOL Mod_CheckDep(json_t *root)
{	
	unsigned int i;
	json_t *value, *deps;
	char *ParentUUID = JSON_GetStr(root, "UUID");
	
	deps = json_object_get(root, "dependencies");
	if (deps == NULL){return TRUE;}
	if(!json_is_array(deps)){return TRUE;}

	json_array_foreach(deps, i, value){
		int ModCount;
		{
			char *command;
			char *uuid = JSON_GetStr(value, "UUID");
			int ver = JSON_GetInt(value, "Ver");
			
			asprintf(&command, "SELECT COUNT(*) FROM Mods "
				     "WHERE UUID = '%s' AND Ver >= %d",
				     uuid, ver
			);
			free(uuid);
			if(command == NULL){
				return FALSE;
			}
			
			ModCount = SQL_GetNum(command);
			free(command);
		}
		
		if (ModCount == 0){
			//There are missing mods. Roll back and abort.
			char *command = NULL;
			
			asprintf(&command,
				"DELETE FROM Dependencies WHERE ParentUUID = \"%s\";",
				ParentUUID
			);
			if(command == NULL){return FALSE;}
			
			sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
			free(command);
			free(ParentUUID);
			return FALSE;
		}
		
		else{
			//Insert that dependency into the table for uninstallation purposes
			char *command = NULL;
			char *uuid = JSON_GetStr(value, "UUID");
			
			asprintf(&command, "INSERT INTO Dependencies "
				"('ChildUUID', 'ParentUUID')  VALUES (\"%s\", \"%s\");",
				uuid, ParentUUID
			);
			free(uuid);
			
			if(command == NULL){return FALSE;}
			sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
			free(command);
		}
	}
	free(ParentUUID);
	return TRUE;
}

//Alert helper function
void Mod_CheckDepAlert(json_t *root){
	char *message = NULL;
	unsigned int messagelen;
	unsigned int i;
	json_t *value, *deps;
	
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
		free(parName);
		free(parAuth);
	}
	
	deps = json_object_get(root, "dependencies");
	if (deps == NULL){return;}
	if(!json_is_array(deps)){return;}
	
	json_array_foreach(deps, i, value){
		int ModCount;
		
		{
			char *command = NULL;
			char *uuid = JSON_GetStr(value, "UUID");
			int ver = JSON_GetInt(value, "Ver");
			
			asprintf(&command, "SELECT COUNT(*) FROM Mods "
				     "WHERE UUID = '%s' AND Ver >= %d",
				     uuid, ver
			);
			free(uuid);
			if(command == NULL){return;}
			
			ModCount = SQL_GetNum(command);
		}
		
		if (ModCount == 0){
			char *name = JSON_GetStr(value, "Name");
			char *auth = JSON_GetStr(value, "Auth");
			int ver = JSON_GetInt(value, "Ver");
			char *out = NULL;
			
			asprintf(&out, "- %s version %d by %s\n", name, ver, auth);
			if(out == NULL){out = strdup("");}
			while(strlen(message) + strlen(out) >= messagelen){
				messagelen *= messagelen;
				message = realloc(message, messagelen);
			}
			strcat(message, out);
			
			free(name);
			free(auth);
			free(out);
		}
	}
	
	AlertMsg(message, "Other Mods Required");
}

///Mod Installation Routines
////////////////////////////////

int File_GetID(char * FileName)
{
	int id, filecount;
	char *command = NULL;
	
	asprintf(&command, "SELECT ID FROM Files WHERE Path = '%s';", FileName);
	id = SQL_GetNum(command);
	filecount = SQL_GetNum("SELECT COUNT(*) FROM Files");
	
	free(command);
	
	if (id > filecount || (id == 0 && filecount == 0)){
		//File is new; record it for future reference
		return File_MakeEntry(FileName);
	}
	
	return id;
}

char * File_GetName(int input)
{
	char *output = NULL, *command = NULL;
	
	asprintf(&command, "SELECT Path FROM Files WHERE ID = %i;", input);
	output = SQL_GetStr(command);
	
	free(command);
	return output;
}

int File_MakeEntry(char *FileName){
	char *command = NULL;
	int ID = SQL_GetNum("SELECT COUNT(*) FROM Files") + 1;
	
	asprintf(&command,
		"INSERT INTO Files (ID, Path) VALUES (%i, '%s')",
		ID, FileName
	);
	sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
	
	free(command);
	return ID;
}

//Effectively this function is a poor-man's string lookup table.
enum VarType Var_GetType(char *type)
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
	free(type);
	return result;
}


struct VarValue Var_GetValue_SQL(char *VarUUID){
	struct VarValue result;
	char *command = NULL;
	json_t *VarArr, *VarObj;
	
	//Get SQL results and put into VarObj
	asprintf(&command, "SELECT Type, Value FROM Variables WHERE UUID = %s", VarUUID);
	VarArr = SQL_GetJSON(command);
	free(command);
	
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
		result.usint = 0; break;
	}
	
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
		free(type);
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
	
	free(value);
	return result;
}

void Var_CreatePubList(json_t *PubList, char *UUID){
	json_t *row;
	unsigned int i;
	
	json_array_foreach(PubList, i, row){
		char *label = JSON_GetStr(row, "Label");
		char *command = NULL;
		int num = JSON_GetInt(row, "Number");
		
		asprintf(&command,
			"INSERT INTO VarList (Var, Number, Label) "
			"VALUES (%s, %d, %s);", UUID, num, label
		);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		
		free(command);
		free(label);
	}	
}

BOOL Var_MakeEntry(json_t *VarObj, char *ModUUID)
{
	char *UUID = NULL, *PubType = NULL;
	struct VarValue result;
	//VarObj is read from mod's file.
	
	//Get variable value and type
	result = Var_GetValue_JSON(VarObj);
	
	//Get a UUID
	UUID = JSON_GetStr(VarObj, "UUID");

	//Check if variable already exists
	if(Var_GetValue_SQL(UUID).type != INVALID){
		//If so, update existing variable instead of making a new one.

		char str[] = "UPDATE Variables SET "
			"'Value' = %? WHERE UUID = %s;";
		int commandlen = strlen(str) + strlen(UUID) + 32;
		char *command = malloc(commandlen);
		
		//Swap out %? for %f, %i or &u depending on type.
		//Compose command too, while we're at it.
		char *charptr = strchr(str,'?');
		switch(result.type){
		case SINT:
			charptr[0] = 'i';
			snprintf(command, commandlen, str, result.sint, UUID);
			break;
		case USINT:
			charptr[0] = 'u';
			snprintf(command, commandlen, str, result.usint, UUID);
			break;
		case IEEE:
			charptr[0] = 'f';
			snprintf(command, commandlen, str, result.ieee, UUID);
			break;
		}
		
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		
		free(command);
		free(UUID);
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
		
		char str[] = "INSERT INTO Variables "
		           "(UUID, Mod, Type, PublicType, Desc, Value) "
			   "VALUES (%s, %s, $s, %s, %s, %?);";
		char *charptr = strchr(str,'?');
		
		//Grab description
		char *Desc = JSON_GetStr(VarObj, "Desc");
		char *command = NULL;
				
		//Swap %s for $f, %d, or %u.
		//Compose command too, while we're at it.
		switch(result.type){
		case SINT:
			charptr[0] = 'i';
			asprintf(&command, str,
				UUID, ModUUID, result.type,
				PubType, Desc, result.sint
			);
			break;
		case USINT:
			charptr[0] = 'u';
			asprintf(&command, str,
				UUID, ModUUID, result.type,
				PubType, Desc, result.usint
			);
			break;
		case IEEE:
			charptr[0] = 'f';
			asprintf(&command, str,
				UUID, ModUUID, result.type,
				PubType, Desc, result.ieee
			);
			break;
		}
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		free(command);
		free(Desc);
	}

	free(UUID);
	free(PubType);
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
struct ModSpace Mod_FindSpace(struct ModSpace input, int len, char *ModUUID, BOOL Clear){
	struct ModSpace result;
	json_t *out, *row;
	char *IDStr = NULL;
	
	//Set defaults for result
	strcpy(result.ID, "");
	result.Start = 0;
	result.End = 0;
	result.FileID = 0;
	
	//Do the finding
	{
		char *command = NULL;
		asprintf(&command, "SELECT * FROM Spaces WHERE Type = '%s' AND UsedBy IS NULL AND "
			"Len >= %i AND Start >= %i AND End <= %i AND File = %i ORDER BY Len;",
			Clear ? "Clear":"Add", len, input.Start, input.End, input.FileID);
		out = SQL_GetJSON(command);
		row = json_array_get(out, 0);
		free(command);
	}
	
	//Make sure there is a space
	if (!json_is_object(row)){return result;}
	
	//Pull the data
	if(JSON_GetStrLen(row, "ID") < 1){
		IDStr = strdup("");
	} else {
		IDStr = JSON_GetStr(row, "ID");
	}
	strcpy(result.ID, IDStr);
	result.FileID = JSON_GetInt(row, "File");
	result.Start = JSON_GetInt(row, "Start");
	result.End = JSON_GetInt(row, "End");
	
	//Drop if we need to.
	if (ModUUID != NULL){
		//AlertMsg(ModUUID, "Test");
		char *NewID = NULL;
		char *command = NULL;

		NewID = malloc(strlen(IDStr) + strlen(".old"));
		strcpy(NewID, IDStr);
		strcat(NewID, ".old");
		
		asprintf(&command,
			"UPDATE Spaces SET UsedBy = '%s', ID = '%s' "
			"WHERE ID = '%s' AND Type = '%s'",
			ModUUID, NewID, IDStr, Clear ? "Clear":"Add"
		);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		free(NewID);
		free(command);
	}
	json_decref(out);
	return result;
}

char * Mod_FindPatchOwner(char *PatchUUID)
{
	char *command = NULL, *out = NULL;
	asprintf(&command, "SELECT UUID FROM Spaces "
		"JOIN Mods ON Spaces.Mod = Mods.UUID "
		"WHERE Spaces.ID = '%s'", PatchUUID);
	out = SQL_GetStr(command);
	free(command);
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
BOOL ModOp_Clear(struct ModSpace input, char *ModUUID){
	int len = input.End - input.Start;
	struct ModSpace Occ;
	char *OccOwner;
	
	//Make sure there isn't anything in the way, first.
	Occ = Mod_FindSpace(input, len, ModUUID, FALSE);
	OccOwner = Mod_FindPatchOwner(Occ.ID);
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
		
		free(errormsg);
		free(OccOwner);
		return FALSE;
	} else if (strcmp(Occ.ID, "") != 0) {
		//Mark space as used
		char *command = NULL;
		char str[] = "INSERT INTO Spaces "
			"(ID, Type, File, Mod, Start, End, Len) "
			"VALUES ('%s', 'Clear', '%d', '%s', '%d', '%d', '%d');";
		
		asprintf(&command, str,
			Occ.ID, input.FileID, ModUUID,
			input.Start, input.End, input.End-input.Start
		);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		
		free(command);
	} else {
		char *ID = NULL, *command = NULL;
		char str[] = "INSERT INTO Spaces "
		             "('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len')  VALUES"
		             "(\"%s\", \"Clear\", %i, \"%s\", %i, %i, %i);";
		
		asprintf(&ID, "%X", input.Start, 16); //Autogen ID from start pos
		asprintf(&command, str, ID,
			input.FileID, ModUUID, input.Start, input.End, len);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		
		free(ID);
		free(command);
	}
	free(OccOwner);
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
void Mod_AddSpace(struct ModSpace input, int start, int len, char *ModUUID){
	//Compute the wanted Start address and End address
	//Tries to go as low as possible, but sometimes the ranges clash.
	int StartAddr = start > input.Start ? start : input.Start;
	int EndAddr = (StartAddr + len) < input.End ? (StartAddr + len) : input.End;
	
	char *command = NULL;
	
	if (strcmp(input.ID, "") == 0){
		itoa(input.Start, input.ID, 16);  //If no ID, make one.
	}
	//Create new AddSpc table row
	asprintf(&command, "INSERT INTO Spaces "
		"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len') VALUES "
		"(\"%s\", \"Add\", %i, \"%s\", %i, %i, %i);",
		input.ID, input.FileID, ModUUID, StartAddr, EndAddr, len);
	
	//Create a ClearSpc entry from [input.start] to [StartAddr] if not equal
	if (input.Start != StartAddr){
		char *temp = NULL;
		int templen = asprintf(&temp, "INSERT INTO Spaces "
			"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len') VALUES "
			"(\"%s\".StartBuff, \"Clear\", %i, \"%s\", %i, %i, %i);",
			input.ID, input.FileID, ModUUID, input.Start, StartAddr, StartAddr - input.Start);
		command = realloc(command, strlen(command) + templen);
		strcat(command, temp);
		free(temp);
	}
	
	//Create a ClearSpc entry from EndAddr to [input.end] if not equal
	if (input.End != EndAddr){
		char *temp = NULL;
		int templen = asprintf(&temp, "INSERT INTO Spaces "
			"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len') VALUES "
			"(\"%s\".EndBuff, \"Clear\", %i, \"%s\", %i, %i, %i);",
			input.ID, input.FileID, ModUUID, EndAddr, input.End, input.End - EndAddr);
		command = realloc(command, strlen(command) + templen);
		strcat(command, temp);
		free(temp);
	}

	//Execute
	sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
	free(command);
	return;
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
BOOL ModOp_Add(struct ModSpace input, int handle, unsigned char * bytes, int len, char *ModUUID, BOOL MakeAdd){
	struct ModSpace FreeSpace;

	//Find some free space to put it in
	FreeSpace = Mod_FindSpace(input, len, ModUUID, TRUE);
	if (FreeSpace.Start - FreeSpace.End == 0){ //If no free space
		AlertMsg ("There is not enough space cleared in the file to install\n"
		          "this mod. Try removing some mods or installing some\n"
		          "space-clearing mods to help make room.\n"
			  "The installation cannot continue.",
		          "Mod Installation Error");
		return FALSE;
	}
	//Replace the ID of the FreeSpace with the input's ID (this is hackish and I should feel bad.)
	strcpy(FreeSpace.ID, input.ID);
	//Only make an add space if not replacing (we'd be adding nothing)
	//if(MakeAdd == TRUE){
		Mod_AddSpace(FreeSpace, input.Start, len, ModUUID);
	//}
	
	//Read existing bytes into revert table and write new bytes in
	{
		unsigned char *OldBytesRaw = NULL;
		char *OldBytes = NULL, *command = NULL;
		int result;
		
		//Read the old bytes
		OldBytesRaw = calloc(len, sizeof(char));
		result = read(handle, OldBytesRaw, len);
		//Quick error checking
		if (result != len){
			AlertMsg("Unexpected amount of bytes read in ModOp_Add.\n"
				"Please report this to mod loader developer.",
				"Mod Loader error");
			return FALSE;
		}
		//Convert to hex string
		OldBytes = Bytes2Hex(OldBytesRaw, len);
		free(OldBytesRaw);
		
		//Construct SQL Statement
		asprintf(&command, 
			"INSERT INTO Revert "
			"('PatchUUID', 'OldBytes') VALUES ('%s', '%s');",
			FreeSpace.ID, OldBytes
		);
		//AlertMsg(command, "SQL COMMAND");
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		
		//Free memory used
		free(OldBytes);
		free(command);
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
			strcpy(ByteStr, "");
			strcpy(ByteMode, "Bytes");
			free(Mode);
		} else {
			free(Mode);
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
		*len = (strlen(ByteStr)) / 2;
		bytes = Hex2Bytes(ByteStr, *len);
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
		
		free(out);
	}*/
	free(ByteStr);
	free(ByteMode);
	return bytes;
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
	#ifdef _WIN32
	HWND ProgDialog;
	#endif
	char ModUUID[128] = "";
	BOOL retval = TRUE;
	strcpy(ModUUID, JSON_GetStr(root, "UUID"));
	
	patchArray = json_object_get(root, "patches");
	
	ProgDialog = ProgDialog_Init(
		json_array_size(patchArray),
		"Installing Mod..."
	);
	
	json_array_foreach(patchArray, i, patchCurr){
		char Mode[16];
		int file, len;
		unsigned char * bytes;
		struct ModSpace input;
		
		//Change directory back to game's root in case it was changed.
		chdir(CURRDIR);
		file = File_OpenSafe(JSON_GetStr(patchCurr, "File"), _O_BINARY|_O_RDWR);
		if(file == -1){return FALSE;}
		
		//Get Patch Mode
		strcpy(Mode, JSON_GetStr(patchCurr, "Mode"));
		
		//Get bytes and error-handle it
		bytes = Mod_GetBytes(patchCurr, &len);
		if (bytes == NULL && 
			!(strcmp(Mode, "Clear") == 0 ||
			strcmp(Mode, "Copy") == 0 ||
			strcmp(Mode, "Move") == 0 )
		){
			char *errormsg = NULL;
			asprintf(&errormsg,
				"This mod appears to have an issue. "
				"Installation cannot continue.\n\n"
				"Please contact the mod developer with "
				"the following information:\n"
				"- The Value parameter is missing on patch no. %i.", i
			);
				   
			AlertMsg(errormsg, "JSON Error");
			free(errormsg);
			return FALSE;
		}

		input.Start = JSON_GetInt(patchCurr, "Start");
		input.End = JSON_GetInt(patchCurr, "End") + 1;
		input.FileID = File_GetID(JSON_GetStr(patchCurr, "File"));
		//We might write to input.ID, so clear it out for later.
		strcpy(input.ID, "");
		
		//Check if Start is a UUID
		if (!json_is_integer(json_object_get(patchCurr, "Start"))){
			//If it is, replace the info with the start/end of the given UUID
			char *command = NULL, *UUID = NULL;
			json_t *out, *row;
			UUID = JSON_GetStr(patchCurr, "Start");
			
			//Get the list
			asprintf(&command, "SELECT Start, End FROM Spaces "
			             "WHERE ID = '%s' AND UsedBy IS NULL;", UUID);
			out = SQL_GetJSON(command);
			row = json_array_get(out, 0);
			free(command);
			
			//Make sure list is not empty
			if(json_array_size(out) == 0){
				asprintf(&command, "Given UUID %s in patch no. %i was not found.", UUID, i);
				AlertMsg(command, "UUID not found");
				free(command);
				return FALSE;
			}
			
			//Parse the list
			input.Start = JSON_GetInt(row, "Start");
			input.End = JSON_GetInt(row, "End");
			
			//If we're doing a replacement, make the new patch
			//take the UUID of the old space.
			if(strcmp(Mode, "Repl") == 0){
				char *ByteStr = JSON_GetStr(patchCurr, "Value");
				strcpy(input.ID, ByteStr);
			}
			
			free(UUID);
			json_decref(out);
		}
		

		//Don't overwrite the new ID if we already set it
		if(strcmp(input.ID, "") == 0){
			strcpy(input.ID, JSON_GetStr(patchCurr, "UUID"));
		}
		//If there was no UUID, make one up. (Hex representation of start)
		if(strcmp(input.ID, "") == 0){
			itoa(input.Start, input.ID, 16);
		}
		//If there's still nothing, I couldn't tell you how.
				
		if(strcmp(Mode, "Repl") == 0){
			if (ModOp_Clear(input, ModUUID) == FALSE){close(file); retval = FALSE; break;}
			if (ModOp_Add(input, file, bytes, len, ModUUID, FALSE) == FALSE){close(file); retval = FALSE; break;}
		}
		else if(strcmp(Mode, "Clear") == 0){
			if (ModOp_Clear(input, ModUUID) == FALSE){close(file); retval = FALSE; break;}
		}
		else if(strcmp(Mode, "Add") == 0){
			if (ModOp_Add(input, file, bytes, len, ModUUID, TRUE) == FALSE){close(file); retval = FALSE; break;}
		}
		//Will add Copy and Move later. Low-priority.
		//Move is higher-priority because that's awesome for finding space
		else{
			AlertMsg("Unknown patch mode", Mode);
			return FALSE;
		}
		
		close(file);
		
		ProgDialog_Update(ProgDialog, 1);
	}
	
	//Add mod to database
	{
		char *command = NULL;
		
		char *uuid = JSON_GetStr(root, "UUID"); 
		char *name = JSON_GetStr(root, "Name");
		char *desc = JSON_GetStr(root, "Desc");
		char *auth = JSON_GetStr(root, "Auth");
		char *date = JSON_GetStr(root, "Date");
		char *cat = JSON_GetStr(root, "Cat");
		int ver = JSON_GetInt(root, "Ver");
		
		char str[] = "INSERT INTO Mods "
			"('UUID', 'Name', 'Desc', 'Auth', 'Ver', 'Date', 'Cat') VALUES "
			"(\"%s\", \"%s\", \"%s\", \"%s\",  %i,   \"%s\", \"%s\");";
		asprintf(&command, str, uuid, name, desc, auth, ver, date, cat);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		
		free(uuid); free(name); free(desc); free(auth); free(date); free(cat);
		free(command);
	}
	ProgDialog_Kill(ProgDialog);	
	return retval;
}

BOOL Mod_FindDep(char *ModUUID)
{
	char *command = NULL;
	int modCount;
	asprintf(&command, "SELECT COUNT(*) FROM Dependencies "
		"JOIN Mods ON Dependencies.ParentUUID = Mods.UUID "
		"WHERE Dependencies.ChildUUID = '%s';", ModUUID);
	modCount = SQL_GetNum(command);
	free(command);
	//AlertMsg(command, "SQL Output");

	return modCount != 0 ? TRUE : FALSE;
}

void Mod_FindDepAlert(char *ModUUID){
	//char errormsg[4096] = "Uninstalling this mod will require "
	//                      "you to uninstall the following:\n\n";
	char *errormsg = NULL, *command = NULL;
	unsigned int i, errorlen = 128;
	json_t *out, *row;
	
	asprintf(&command, "SELECT Name, Auth, Ver FROM Dependencies "
		"JOIN Mods ON Dependencies.ParentUUID = Mods.UUID "
		"WHERE Dependencies.ChildUUID = '%s';", ModUUID);
	out = SQL_GetJSON(command);
	free(command);
	
	errormsg = malloc(errorlen);
	strcpy(errormsg, "Uninstalling this mod will require "
	                 "you to uninstall the following:\n\n");
	
	json_array_foreach(out, i, row){
		char *temp = NULL;
		int templen;

		//Print it into a string
		templen = asprintf(&temp, "- %s version %s by %s\n",
			JSON_GetStr(row, "Name"),
			JSON_GetStr(row, "Ver"),
			JSON_GetStr(row, "Auth")
		);
		
		while(strlen(errormsg) + templen >= errorlen){
			errorlen *= errorlen; errormsg = realloc(errormsg, errorlen);
		}
		
		//Add it to the error message
		strcat(errormsg, temp);
		free(temp);
	}
	//TODO: Prompt user yes/no, then uninstall in order (not in this funct.)
	AlertMsg(errormsg, "Other Mods Preventing Uninstallation");
	free(errormsg);
	json_decref(out);
	return;
}

//Write back the old bytes and then remove the space.
void ModOp_UnAdd(json_t *row)
{
	int filehandle, offset, datalen;
	unsigned char *bytes;
	char *ByteStr;
	
	//Open file handle
	{
		char *filename = File_GetName(JSON_GetInt(row, "File"));
		filehandle = File_OpenSafe(filename, _O_BINARY|_O_RDWR);
		free(filename);
	}
	if(filehandle == -1){return;} //Failure. Just... ignore it.
	
	//Get offset & length
	offset = JSON_GetInt(row, "Start"); 
	datalen = JSON_GetInt(row, "Len");
	
	ByteStr = malloc(datalen*2);
	assert(ByteStr != NULL);
	
	//Retrieve data
	{
		char *command = NULL;
		char *PatchUUID = JSON_GetStr(row, "ID");
		
		asprintf(&command,
			"SELECT OldBytes FROM Revert WHERE PatchUUID = '%s';",
			PatchUUID
		);
		ByteStr = SQL_GetStr(command);
		
		free(command);
		free(PatchUUID);
	}

	//Convert data string from hex to raw bytes
	bytes = Hex2Bytes(ByteStr, datalen);
	
	//Write back data
	//File_WriteBytes(filehandle, offset, bytes, datalen);
	
	//Remove the space and the old bytes
	ModOp_UnClear(row); //Efficiency!
	{
		char str[] = "DELETE FROM Revert WHERE PatchUUID = '%s';";
		char *PatchUUID = JSON_GetStr(row, "ID");
		int commandlen = strlen(str) + strlen(PatchUUID);
		char *command = malloc(commandlen);
		snprintf(command, commandlen, str, PatchUUID);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		free(command);
	}
	free(bytes);
	free(ByteStr);
	close(filehandle);
	return;
}

//Just remove the space. (We are uninstalling sequentially for a reason.)
void ModOp_UnClear(json_t *row)
{
	char *command = NULL;
	char *PatchUUID = JSON_GetStr(row, "ID");
	
	asprintf(&command, "DELETE FROM Spaces WHERE ID = '%s';", PatchUUID);
	sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
	
	free(command);
	free(PatchUUID);
	return;
}

BOOL Mod_Uninstall(char *ModUUID)
{
	int modCount, i, modStop;
	//Define progress dialog (handle is shell-specific)
	#ifdef _WIN32
	HWND ProgDialog;
	#endif
	
	chdir(CURRDIR); //In case path was changed during installation or w/e.
	
	//Check for dependencies
	if(Mod_FindDep(ModUUID) == TRUE){
		Mod_FindDepAlert(ModUUID);
		return FALSE;
	};
	
	//Count mods
	modCount = SQL_GetNum("SELECT COUNT(*) FROM Mods;");
	
	//Get stopping point
	{
		char *command = NULL;
		asprintf(&command,
			"SELECT RowID FROM Mods WHERE UUID = '%s';", ModUUID
		);
		modStop = SQL_GetNum(command);
		free(command);
	}
	
	//Init progress box
	ProgDialog = ProgDialog_Init(modCount, "Uninstalling Mod...");
	
	//Perform operations in reverse
	for (i = modCount; i >= modStop; i--){

		//Get list of operations
		char *command = NULL;
		json_t *out, *row;
		unsigned int j;
		
		asprintf(&command, "SELECT Spaces.* FROM Spaces "
			"JOIN Mods ON Spaces.Mod = Mods.UUID "
			"WHERE Mods.RowID = %i;", i
		);
		out = SQL_GetJSON(command);
		free(command);
		
		//Loop through operations
		json_array_foreach(out, j, row){
			//Find type
			char *SpaceType = JSON_GetStr(row, "Type");
			
			if(strcmp(SpaceType, "Add") == 0){
				ModOp_UnAdd(row);
			} else if (strcmp(SpaceType, "Clear") == 0) {
				ModOp_UnClear(row);
			}
			free(SpaceType);
		}
		json_decref(out);
		
		//Mod has now been uninstalled.
		//Unmark used spaces
		{
			char str[] = "UPDATE Spaces SET UsedBy = NULL WHERE "
			             "UsedBy = (SELECT UUID FROM Mods WHERE RowID = %i);";
			int commandlen = strlen(str) + 8;
			char *command = malloc(commandlen);
			snprintf(command, commandlen, str, i);
			sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
			free(command);
		}
		
		//Remove mod entry
		{
			char str[] = "DELETE FROM Mods WHERE RowID = %i;";
			int commandlen = strlen(str) + 8;
			char *command = malloc(commandlen);
			snprintf(command, commandlen, str, i);
			sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
			free(command);
		}
		
		//Increment progress bar
		ProgDialog_Update(ProgDialog, 1);
	}
	
	//Kill progress box
	ProgDialog_Kill(ProgDialog);
	
	//Reinstall existing mods
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

void SQL_Load(){
	int result;

	//Open database
	result = sqlite3_open("test.db", &CURRDB);
	assert(result == SQLITE_OK);
	
	//Create tables
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
	);
		
	sqlite3_exec(CURRDB,
		"CREATE TABLE IF NOT EXISTS `Mods` ("
		"`UUID`	TEXT NOT NULL UNIQUE PRIMARY KEY,"
		"`Name`	TEXT NOT NULL,"
		"`Desc`	TEXT,"
		"`Auth`	TEXT,"
		"`Ver` 	INTEGER NOT NULL,"
		"`Date`	TEXT,"
		"`Cat` 	TEXT );",
		NULL, NULL, NULL
	);
	
	sqlite3_exec(CURRDB,
		"CREATE TABLE IF NOT EXISTS `Dependencies` ("
		"`ParentUUID`	TEXT NOT NULL,"
		"`ChildUUID` 	TEXT NOT NULL);",
		NULL, NULL, NULL
	);
	
	sqlite3_exec(CURRDB,
		"CREATE TABLE IF NOT EXISTS `Revert` ("
		"`PatchUUID`	TEXT NOT NULL,"
		"`OldBytes` 	TEXT NOT NULL);",
		NULL, NULL, NULL
	);
	
	sqlite3_exec(CURRDB,
		"CREATE TABLE IF NOT EXISTS `Files` ("
		"`ID`  	INTEGER NOT NULL,"
		"`Path`	TEXT NOT NULL);",
		NULL, NULL, NULL
	);
}

void SQL_Populate(json_t *GameCfg)
{
	//char *out = NULL;
	json_t *spaceArray, *spaceCurr;
	size_t i;
	int spaceCount;
	
	//Don't populate if we don't need to
	spaceCount = SQL_GetNum("SELECT COUNT(*) FROM Spaces");
	if (spaceCount != 0){return;}
	
	//To decrease disk I/O and increase speed
	sqlite3_exec(CURRDB, "BEGIN TRANSACTION", NULL, NULL, NULL);

	spaceArray = json_object_get(GameCfg, "KnownSpaces");
	json_array_foreach(spaceArray, i, spaceCurr){
		int start, end, commandlen;
		char *command;
		char str[] = "INSERT INTO Spaces "
			"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len')  VALUES"
			"(\"%s\", \"Add\", %i, \"%s\", %i, %i, %i);";
		
		start = JSON_GetInt(spaceCurr, "Start");
		end = JSON_GetInt(spaceCurr, "End");
	
		commandlen = strlen(str) + 64 +
			strlen(JSON_GetStr(spaceCurr, "UUID"));
		command = malloc(commandlen);
		snprintf(command, commandlen, str,
			JSON_GetStr(spaceCurr, "UUID"), 0, "",
			start, end, end-start
		);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		free(command);
	}
	sqlite3_exec(CURRDB, "COMMIT", NULL, NULL, NULL);
}

// Main Dialog Procedure
BOOL CALLBACK Dlg_Main(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message){
	case WM_INITDIALOG:{
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
		{
			int ErrorCode = 0;
			do {
				ErrorCode = SelectInstallFolder(GameCfg);
				if(ErrorCode == -1){PostQuitMessage(0); return TRUE;}
			} while (ErrorCode != 0);
			//GamePath is now CURRDIR
		}
		
		SQL_Load();
		SQL_Populate(GameCfg);
		
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
		case IDLOAD:{
			json_t *jsonroot;
				
			//Get name of mod to load
			char fpath[MAX_PATH] = ""; // Path mod is located at
			char fname[MAX_PATH] = ""; // Filename of mod
	
			SelectFile(hwnd, &fpath, &fname, "Mod Metadata (.json)\0*.json\0\0");
			//If no path given, quit
			if(strcmp(fpath, "") == 0){return TRUE;}
	
			/// Copy mod to [path]/mods/[name]
			// Set destination
			// Actually copy w/ Windows API
			
			///Load
			jsonroot = JSON_Load(&fpath);
			if (jsonroot == NULL){break;}
			
			///Verify
			if (Mod_CheckCompat(jsonroot) == FALSE){
				json_decref(jsonroot);
				break;
			}
			if (Mod_CheckConflict(jsonroot) != 0){
				json_decref(jsonroot);
				break;
			}
			if (Mod_CheckDep(jsonroot) == FALSE){
				Mod_CheckDepAlert(jsonroot);
				json_decref(jsonroot);
				break;
			}
		
			///Install mod
			//Disable window during install
			EnableWholeWindow(hwnd, FALSE);
			
			if (Mod_Install(jsonroot) == FALSE){
				char *ModUUID = JSON_GetStr(jsonroot, "UUID");
				Mod_Uninstall(ModUUID);
				free(ModUUID);
			};
			
			///Update mod list
			UpdateModList(GetDlgItem(hwnd, IDC_MAINMODLIST));
			UpdateModDetails(hwnd, SendMessage(
					GetDlgItem(hwnd, IDC_MAINMODLIST),
					LB_GETCURSEL, 0, 0)
				);
			
			//Re-enable window
			EnableWholeWindow(hwnd, TRUE);
			json_decref(jsonroot);
			break;
			}
		
		case IDREMOVE:{
			char *ModName;
			char *ModUUID;
			int len, CurrSel;
			HWND list;
			
			//Get the handle for the mod list
			list = GetDlgItem(hwnd, IDC_MAINMODLIST);
			
			//Get the current selection
			CurrSel = SendMessage(list, LB_GETCURSEL, 0, 0);
			if(CurrSel == -1){break;}
			
			//Get the length of the name of the selected mod
			len = SendMessage(list, LB_GETTEXTLEN, CurrSel, 0);
			
			//Allocate space for the name
			ModName = malloc(len);
			if(ModName == NULL){break;}
			
			//Actually get the name
			SendMessage(list, LB_GETTEXT, CurrSel, (LPARAM)ModName);
			
			//Use that to get the UUID
			{
				char *command = NULL;
				int commandlen;
				char str[] = "SELECT UUID FROM Mods WHERE "
				             "Name = '%s';";
				commandlen = strlen(str) + strlen(ModName);
				command = malloc(commandlen);
				
				snprintf(command, commandlen, str, ModName);
				ModUUID = SQL_GetStr(command);
				free(command);
			}			
			free(ModName);
			
			//Finally remove the mod
			EnableWholeWindow(hwnd, FALSE);
			Mod_Uninstall(ModUUID);
			free(ModUUID);
			
			//Update UI
			UpdateModList(GetDlgItem(hwnd, IDC_MAINMODLIST));
			UpdateModDetails(hwnd, SendMessage(
					GetDlgItem(hwnd, IDC_MAINMODLIST),
					LB_GETCURSEL, 0, 0)
				);
			EnableWholeWindow(hwnd, TRUE);
		}
		
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
	json_set_alloc_funcs(malloc, free);

	if (!hDialog){
		char buf [100];
		sprintf (buf, "Error x%x.", GetLastError());
		AlertMsg (buf, "CreateDialog");
		return 1;
	}


	while ((status = GetMessage (& msg, 0, 0, 0)) != 0){
		if (status == -1) {return -1;};
		if (!IsDialogMessage (hDialog, & msg)){
			TranslateMessage ( & msg );
			DispatchMessage ( & msg );
		}
	}
	sqlite3_close(CURRDB);
	return msg.wParam;
}
