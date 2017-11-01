/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>                    // Malloc and other useful things
#include <errno.h>                     // Catching and explaining errors
#include <io.h>                        // Standardized I/O library.
                                       // Not using the Win32 APIs for this in case
                                       // somebody wants to port the back-end
// These are for access() in io.h. They're defined somewhere I'm sure
// but I couldn't tell you where.
#define  F_OK 0 	               // Check for existence
#define  X_OK 1 	               // Check for execute permission
#define  W_OK 2 	               // Check for write permission
#define  R_OK 4 	               // Check for read permission
				 
#include <fcntl.h>                     // Flags for open() (O_RDWR, O_RDONLY)
#include "unistd.h"                       // Switch and manipulate directories
#include <assert.h>                    // Debugging.


#include <jansson.h>                   // EXTERNAL (http://www.digip.org/jansson/)
                                       // JSON parser. Used to load patches and mod info
				       // from a single file with ease.
#include <sqlite3.h>                   // EXTERNAL (http://sqlite.org)
                                       // Embedded database. Used for quick lookups
				       // of existing mods, dependencies, variables, etc.
				      
				
#include <windows.h>                   // Win32 APIs for the frontend
#include <commctrl.h>                  // TreeView control, so mods can have parents
#include <Shlobj.h>                    // "Open Folder" dialog 

#include "resource.h"                  // Defines IDs so we can access resources
#include "funcproto.h"                // Function prototypes and structs
				    
#define PROGVERSION_MAJOR 1            // Major version. Compatibility-breaking.
#define PROGVERSION_MINOR 0            // Minor version. Shiny new features.
#define PROGVERSION_BUGFIX 0           // Bugfix release. For bugfixes and stuff.
const PROGVERSION = PROGVERSION_BUGFIX + (PROGVERSION_MINOR*100) + (PROGVERSION_MAJOR*10000);
const char PROGAUTHOR[] = "InvisibleUp"; //Name of program author (don't change please.)
const char PROGSITE[] = "http://github.com/InvisibleUp/SrModLdr/"; //Site to get new revisions (don't change please.) 

char CURRDIR[MAX_PATH] = ""; //Current root directory of game
sqlite3 *CURRDB; //Current database holding patches 

//Temporary until we get a config file going
const char GAMECONFIG[] = ".\\games\\sonicr.json";
/*
const char GAMENAME[] = "Sonic R";
const char GAMEEXE[] = "SONICR.EXE";


const int ValidEXECount = 4;				       
const struct ValidEXE ValidEXEList[] = {
	// Vanilla EXE (Expert Software, probably others)
	{1263104, 0x1a3dd1bc, NULL, NULL},
	// "" with CPU bugfix only
	{1263104, 0x922323e1, "cpufix@invisibleup", NULL},
	// "" with No-CD patch only
	{1263104, 0x341828e8, "COMPAT-NOCD", NULL},
	// "" with CPU bugfix and No-CD patch	
	{1263104, 0xd3906127, "cpufix@invisibleup", "COMPAT-NOCD"}
};*/
/* Blacklist
	// Network Patch (dl from Sonic Retro)
	{1077248,0xcf94dbd3,,}
	// Network Patch (dl from shady message board) 
	// (very minor diff between this and Twin Pack ed. in CPU code.)
	{1074176,0x8b4c3c2f,,}
	// Network Patch (Activision Twin Pack on-disk patch [debugging ver?])
	{1074176,0xbd41908e,,}
*/

//int FileIDCount = 0;
/*struct FileID *FileIDList[] = {
	{0, "SONICR.EXE"},
	{1, "unknown"}
};*/
	
char GAMEPATH[MAX_PATH] = "";  // Sonic R install path

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
	HWND hCtl;
	
	EnableWindow(hwnd, state);
	UpdateWindow(hwnd);
	
	//Disable all the controls, too.
	hCtl = GetWindow(hwnd,GW_CHILD);
	while (hCtl) {
		EnableWindow(hCtl, state);
		UpdateWindow(hCtl);
		hCtl = GetWindow(hCtl, GW_HWNDNEXT);
	}
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
	int i;

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
		name = strdup(JSON_GetStr(row, "Name"));
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
	char *command;
	int commandlen;
	json_t *out, *row;
	char *name = NULL, *auth = NULL, *cat = NULL, *desc = NULL, *ver = NULL;
	return;
	//Select the right mod
	commandlen = snprintf(NULL, 0 , "SELECT * FROM Mods WHERE RowID = %d;", listID+1);
	command = malloc(commandlen);
	snprintf(command, commandlen, "SELECT * FROM Mods WHERE RowID = %d;", listID+1);
	
	out = SQL_GetJSON(command);
	row = json_array_get(out, 0);
	
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

	//Get used EXE space
	{
		char EXESpc[64] = "";
		char *UUID = JSON_GetStr(row, "UUID");
		itoa(GetUsedSpaceNum(UUID, 0), EXESpc, 10);
		strcat(EXESpc, " bytes");
		
		//Write to dialog
		SetWindowText(GetDlgItem(hwnd, IDC_MAINEXESPACE), EXESpc);
	}
	json_decref(row);
	json_decref(out);
	return;
}

long GetUsedSpaceNum(char *ModUUID, int File)
{
	int add, clear;
	char command[1024] = "";
	
	sprintf(command,"SELECT SUM(Len) FROM Spaces WHERE "
		"File = %i AND Mod = '%s' AND Type = 'Add';",
		File, ModUUID);
	add = SQL_GetNum(command);
	
	sprintf(command,"SELECT SUM(Len) FROM Spaces WHERE "
		"File = %i AND Mod = '%s' AND Type = 'Clear';",
		File, ModUUID, ModUUID);
	//AlertMsg(command, "COMMAND");
	clear = SQL_GetNum(command);

	return add - clear;
}

///SQLite helper Functions

//I'm sorry, I'm not typing all this out every time.
char * SQL_ColName(sqlite3_stmt * stmt, int col){
	return sqlite3_column_name(stmt, col) == NULL ?\
	"" : sqlite3_column_name(stmt, col);
}

char * SQL_ColText(sqlite3_stmt * stmt, int col){	
	return sqlite3_column_text(stmt, col) == NULL ?\
	"" : sqlite3_column_text(stmt, col);
}

json_t * SQL_GetJSON(char *query)
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
	
	//(Actually, just get me a list of columns and then we'll talk.)
	errorNo = sqlite3_step (stmt);
        do {
		//Never can be too safe, you know
		while(strlen(JSONStr) + 1 > len){
			len *= len; JSONStr = realloc(JSONStr, len);
		}
		strcat(JSONStr, "{ ");
		
		for(i = sqlite3_column_count(stmt) - 1; i >= 0; i--){
			char *temp = NULL;
			int templen = 8;
			templen += strlen(SQL_ColName(stmt, i));
			templen += strlen(SQL_ColText(stmt, i));
			while(templen + strlen(JSONStr) > len){
				len *= 2; JSONStr = realloc(JSONStr, len);
			}
			temp = malloc(templen);
			
			if(strcmp(SQL_ColText(stmt, i), "") == 0){
				sprintf(temp, "\"%s\":null,", SQL_ColName(stmt, i));
			} else if (strcmp(sqlite3_column_decltype(stmt, i), "INTEGER") == 0) {
				sprintf(temp, "\"%s\":%s,", SQL_ColName(stmt, i), SQL_ColText(stmt, i));
			} else {
				sprintf(temp, "\"%s\":\"%s\",", SQL_ColName(stmt, i), SQL_ColText(stmt, i));
			}
			
			strcat(JSONStr, temp);
		}
		
		while(strlen(JSONStr) + 1 > len){
			len *= 2; JSONStr = realloc(JSONStr, len);
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
		char errMsg[256];
		sprintf(errMsg, "line %d: %s\nContact mod loader dev.",
			error.line, error.text
		);
		AlertMsg(errMsg, "SQL->JSON Error");
	}
	
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
		char buf [256];
		sprintf (buf,
			"An error has occurred loading mod metadata.\n\n"
			"Contact the mod developer with the following info:\n"
			"line %d: %s", error.line, error.text);
		AlertMsg (buf, "Invalid JSON");
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
 *         Name:  JSON_GetStr
 *  Description:  Gives a string given a JSON tree and the name of a tag
 * =====================================================================================
 */
const char * JSON_GetStr(json_t *root, char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL) {return "";}
	if (json_is_integer(object)) {
		//WARNING: Possible memory leak if used improperly
		char *out = malloc(16);
		itoa(JSON_GetInt(root, name), out, 10);
		return out;
	}
	return json_string_value(object);
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
	if (object == NULL) {return 0;}
	if (json_is_integer(object)) {
		//Don't ask me how to free this.
		char *out = malloc(64);
		int result;
		
		itoa(JSON_GetInt(root, name), out, 10);
		AlertMsg(out, "Out");
		result = strlen(out);
		free(out);
		return result;
	}
	return json_string_length(object);
}

///Generic I/O Helper Functions

/* 
 * ===  MACRO  =========================================================================
 *         Name:  arrlen
 *  Description:  Simple macro to give the size of any array assuming it's size is known
 * =====================================================================================
 */
#define arrlen(array) (sizeof(val)/sizeof(val[0]))

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
			char buf [256];
			if(InFolder){
				sprintf (buf,
					"This folder does not contain %s.\n"
					"Please choose one that contains '%s'\n"
					"and all other required files.", file, file);
			}
			else{
				sprintf (buf,
					"The file %s could not be found.\n"
					"Please choose a different file.", file);
			}
			AlertMsg (buf, "File not Found");
			return FALSE;
		}
		case EACCES:{ // Read only error
			char buf [256];
			sprintf (buf,
			"This %s is read-only. Make sure this %s is\n"
			"on your hard disk and not on a CD, as this program\n"
			"cannot work directly on a CD.", objName, objName);
			AlertMsg (buf, "Read-Only");
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
BOOL File_Known(char FileName[MAX_PATH], char GameName[128], int handle, json_t *whitelist)
{	
	size_t fsize, fchecksum;
	size_t i;
	json_t *value;
	BOOL SizeOK = FALSE, CRCOK = FALSE;
	
	//Get filesize
	fsize = filelength(handle);
	//Get the checksum of the file
	fchecksum = crc32(handle, fsize);
/*	{
		char buf[16] = "";
		itoa(fchecksum, buf, 16);
		MessageBox(0, buf, "Checksum result", MB_OK);
	}
*/	//Make sure the file is actually valid via whitelist
/*	for (i = 0; i < ValidEXECount; i++){
		if (fsize == ValidEXEList[i].size){
			errorcode = 0;
			break;
		} else { errorcode = 1; }
	}*/
	json_array_foreach(whitelist, i, value){
		size_t ChkSum = strtol(JSON_GetStr(value, "CRC32"), NULL, 16);
		size_t Size = JSON_GetInt(value, "Size");

		if (fsize == Size){ SizeOK = TRUE; }
		if (fchecksum == ChkSum){ CRCOK = TRUE; }
	}
	

	//Search through list
	/*for (i = 0; i < ValidEXECount; i++){
		if (checksum == ValidEXEList[i].crc32){
			errorcode = 0;
			break;
		} else { errorcode = 1; }
	}*/
	
	//Return if the file is unknown
	if (SizeOK == FALSE || CRCOK == FALSE){
		char buf [256];
		sprintf (buf,
			"The %s file does not match any known version.\n"
			"This means you have an incompatible version of %s.\n"
			"Make sure you have the right revision!",
			FileName, GameName);
		AlertMsg (buf, "Incompatible File");
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
		char buf [256];
		sprintf (buf,
			"An unknown error occurred when opening %s.\n"
			"Make sure your system is not low on resources and try again.\n",
			filename);
		AlertMsg (buf, "General I/O Error");
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
 * =====================================================================================
 */
const char * SelectInstallFolder(json_t *GameCfg)
{
	int handle;
	char path[MAX_PATH] = "";
	char title[128] = "";
	char GameName[128];
	char GameEXE[128];
	
	strcpy(GameName, JSON_GetStr(GameCfg, "GameName"));
	strcpy(GameEXE, JSON_GetStr(GameCfg, "GameEXE"));

	strcat (title, "Pick ");
	strcat (title, GameName);
	strcat (title, " Installation directory.");
	
	strcpy(path, SelectFolder(title)); //Pick directory
		
	// User refuses to give a directory
	if (strcmp(path, "") == 0) {
		//if(strcmp(CURRDIR, "") == 0){PostQuitMessage(0);} //Wrong program!
		return "QUITMSG"; //Wrong menu option!
	}
	
	strcpy(CURRDIR, path); // Set directory as current directory
	chdir(CURRDIR); // Move to the directory chosen.
	
	// Make sure it has the game file
	if(!File_Exists(GameEXE, TRUE, TRUE)){return "";}
	
	//Now we open the file so we can read it.
	handle = File_OpenSafe(GameEXE, _O_BINARY|_O_RDWR);
	if (handle == -1){return "";};
	
	//Check if file is known
	{
		BOOL result = File_Known(GameEXE, GameName, handle, json_object_get(GameCfg, "Whitelist"));
		if(result == FALSE){strcpy(path, "");}
	}
	
	//Close handle (this cannot fail)
	close(handle); 
	
	return path; //The path is good! Return it!
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
	char ML_Str[64];
	
	strcpy(ML_Str, JSON_GetStr(root, "ML_Ver"));
	// Make sure there even is a version string
	if (ML_Str == NULL){	
		AlertMsg ("An error has occurred loading mod metadata.\n\n"
			"Contact the mod developer with the following info:\n"
			"No mod loader version string was found.\n"
			"It should be named 'ML_Ver' and be in the format\n"
			"[MAJOR].[MINOR].[BUGFIX]",
			"Invalid JSON");
		return FALSE;	
	}
	error_no = sscanf(ML_Str, "%d.%d.%d", &ML_Maj, &ML_Min, &ML_Bfx);
	
	//Make sure version string is formatted properly
	if (error_no != 3){
		AlertMsg (	"This mod is trying to ask for an invalid mod loader version.\n\n"
			"Please contact the mod developer with the following information:\n"
			"ML_Ver string is not in format [MAJOR].[MINOR].[BUGFIX]",
			"Invalid Version Requested");
		return FALSE;
	}

	//Lazy arithmetic to compare versions.
	ML_Ver = ML_Bfx + (ML_Min*100) + (ML_Maj*10000);
	if(ML_Ver > PROGVERSION){
		char buf [256];
		sprintf (buf,
			"This mod requires a newer version of the mod loader.\n"
			"You have version %d.%d.%d and need version %d.%d.%d.\n"
			"Obtain an updated copy at\n%s",
			PROGVERSION_MAJOR, PROGVERSION_MINOR, PROGVERSION_BUGFIX,
			ML_Maj, ML_Min, ML_Bfx, PROGSITE);
		AlertMsg (buf, "Outdated Mod Loader");
		return FALSE;
	}
	return TRUE;
}

int Mod_CheckConflict(json_t *root)
{
	// Get UUID and name
	char name[64];
	char UUID[64];
	int oldver = 99;
	strcpy(UUID, JSON_GetStr(root, "UUID"));
	strcpy(name, JSON_GetStr(root, "Name"));
	assert(UUID != NULL);
	assert(name != NULL);
	
	// Check for conflicts
	{
		char command[256];
		sprintf(command, "SELECT Ver FROM Mods WHERE UUID = '%s';", UUID);
		oldver = SQL_GetNum(command);
	}
	if (oldver != 0){
		int MBVal, ver; // Message box response, version req.
		char buf [256]; // Messageboxes
		//Get name
		
		//Check version number
		ver = JSON_GetInt(root, "Ver");
		if(ver > oldver){
			sprintf (buf, 
				"The mod you have selected is a newer version of\n"
				"'%s', which you have already installed.\n"
				"Do you want to upgrade from version %d to version %d?",
				name, oldver, ver);
			MBVal = MessageBox (0, buf, "Mod Upgrade", MB_ICONEXCLAMATION | MB_YESNO);
		}
		else if (ver < oldver){
			sprintf (buf, 
				"The mod you have selected is an OLDER version of\n"
				"'%s', which you have already installed.\n"
				"Do you want to downgrade from version %d to version %d?",
				name, oldver, ver);
			MBVal = MessageBox (0, buf, "Mod Downgrade", MB_ICONEXCLAMATION | MB_YESNO);
		}
		else{
			sprintf (buf, "%s is already installed.", name);
			AlertMsg (buf, "Mod Already Installed");
			MBVal = IDNO;
		}
		if (MBVal == IDNO){return 2;}
		else{return 1;}
	}
	
	return 0;
}

BOOL Mod_CheckDep(json_t *root)
{	
	char errormsg[1024];
	BOOL error = FALSE;
	size_t i;
	json_t *value, *array;
	
	array = json_object_get(root, "dependencies");
	if (array == NULL){return TRUE;}
	if(!json_is_array(array)){return TRUE;}
	
	sprintf(errormsg,
		"%s version %d by %s requires\nsome other mods that you do not have:\n\n",
		JSON_GetStr(root, "Name"),
		JSON_GetInt(root, "Ver"),
		JSON_GetStr(root, "Auth")
	);
				  
	json_array_foreach(array, i, value){
		char command[256];
		int modCount;

		sprintf(command, "SELECT COUNT(*) FROM Mods "
                             "WHERE UUID = '%s' AND Ver >= %d",
			     JSON_GetStr(value, "UUID"), JSON_GetInt(value, "Ver")
		);
		modCount = SQL_GetNum(command);
		
		if (modCount != 1){ //There are missing mods
			char out[512];
			sprintf(out, "- %s version %d by %s\n",
				JSON_GetStr(value, "Name"),
				JSON_GetInt(value, "Ver"),
				JSON_GetStr(value, "Auth")
			);
			strcat(errormsg, out);
			error = TRUE;
		}
		else{
			//Insert that dependency into the table for uninstallation
			sprintf(command, "INSERT INTO Dependencies "
				"('ChildUUID', 'ParentUUID')  VALUES (\"%s\", \"%s\");",
				JSON_GetStr(value, "UUID"),
				JSON_GetStr(root, "UUID")
			);
			sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		}
	}
	if (error == TRUE){
		//Purge any dependency records we've written.
		char command[1024] = "";
		sprintf(command,
			"DELETE FROM Dependencies WHERE ParentUUID = \"%s\";",
			JSON_GetStr(root, "UUID")
		);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		
		AlertMsg(errormsg, "Other Mods Required");
		return FALSE;
	}
	return TRUE;
}

///Mod Installation Routines
////////////////////////////////

int GetFileID(char * FileName)
{
	int id, filecount;
	char str[] = "SELECT ID FROM Files WHERE Path = '%s';";
	int commandlen = strlen(str) + strlen(FileName);
	char *command = malloc(commandlen);
	snprintf(command, commandlen, str, FileName);
	
	id = SQL_GetNum(command);
	filecount = SQL_GetNum("SELECT COUNT(*) FROM Files");
	free(command);
	if (id > filecount || (id == 0 && filecount == 0)){
		//File is new; record it for future reference
		return MakeFileEntry(FileName);
	}
	return id;
}

char * GetFileName(int input)
{
	char *output;
	char str[] = "SELECT Path FROM Files WHERE ID = %i;";
	int commandlen = strlen(str) + 8;
	char *command = malloc(commandlen);
	snprintf(command, commandlen, str, input);
	
	output = strdup(SQL_GetStr(command));
	free(command);
	return output;
}

int MakeFileEntry(char *FileName){
	char str[] = "INSERT INTO Files (ID, Path) VALUES (%i, '%s')";
	int ID = SQL_GetNum("SELECT COUNT(*) FROM Files") + 1;
	int commandlen = strlen(str) + strlen(FileName) + 8;
	char *command = malloc(commandlen);
	snprintf(command, commandlen, str, ID, FileName);
	sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
	free(command);
	return ID;
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
	struct ModSpace result = {0};
	char command[256] = "";
	json_t *out, *row;
	char *IDStr = NULL;
	
	//Do the finding
	sprintf(command, "SELECT * FROM Spaces WHERE Type = '%s' AND UsedBy IS NULL AND "
			"Len >= %i AND Start >= %i AND End <= %i AND File = %i "
			"ORDER BY Len;", Clear ? "Clear":"Add", len, input.Start, input.End, input.FileID);
	out = SQL_GetJSON(command);
	row = json_array_get(out, 0);

	//Make sure there is one
	if (!json_is_object(row)){return result;}
	//Pull the data
	if(JSON_GetStrLen(row, "ID") < 1){
		IDStr = strdup("");
	} else {
		IDStr = strdup(JSON_GetStr(row, "ID"));
	}
	strcpy(result.ID, IDStr);
	result.FileID = JSON_GetInt(row, "File");
	result.Start = JSON_GetInt(row, "Start");
	result.End = JSON_GetInt(row, "End");
	
	//Drop if we need to.
	if (ModUUID != NULL){
		//AlertMsg(ModUUID, "Test");
		char *NewID = NULL;

		NewID = malloc(strlen(IDStr) + strlen(".old"));
		strcpy(NewID, IDStr);
		strcat(NewID, ".old");
		
		sprintf(command,
			"UPDATE Spaces SET UsedBy = '%s', ID = '%s' "
			"WHERE ID = '%s' AND Type = '%s'",
			ModUUID, NewID, IDStr, Clear ? "Clear":"Add"
		);
		free(NewID);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
	}
	json_decref(row);
	json_decref(out);
	return result;
}

char * Mod_FindPatchOwner(char *PatchUUID)
{
	char *command = NULL, *out = NULL;
	int commandlen;
	char str[] = "SELECT UUID FROM Spaces "
		"JOIN Mods ON Spaces.Mod = Mods.UUID "
		"WHERE Spaces.ID = '%s'";
	commandlen = strlen(str) + strlen(PatchUUID);	
	command = malloc(commandlen);
	snprintf(command, commandlen, str, PatchUUID);
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
	
	if (strcmp(OccOwner, "") != 0){ //If space is occupied
		//Check owning mod
		char *errormsg = NULL;
		int errorlen;
		char str[] = "The mod is requesting to clear space that is\n"
			"currently occupied by patch %s.\n\n"
			"The installation cannot continue.\n"
			"Remove the offending mod and try again.";
		
		errorlen = strlen(str) + strlen(OccOwner);
		errormsg = malloc(errorlen);
		snprintf(errormsg, errorlen, str, OccOwner);
		
		AlertMsg (errormsg, "Mod Installation Error");
		free(errormsg);
		return FALSE;
	} else if (strcmp(Occ.ID, "") != 0) {
		//Mark space as used
		char str[] = "INSERT INTO Spaces "
			"(ID, Type, File, Mod, Start, End, Len) "
			"VALUES ('%s', 'Clear', '%d', '%s', '%d', '%d', '%d');";
		int commandlen = strlen(str) + strlen(Occ.ID) + strlen(ModUUID) + 20;
		char *command = malloc(commandlen);
		
		snprintf(command, commandlen, str,
			Occ.ID, input.FileID, ModUUID,
			input.Start, input.End, input.End-input.Start
		);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		free(command);
	} else {
		char ID[32] = "";
		char str[] = "INSERT INTO Spaces "
		             "('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len')  VALUES"
		             "(\"%s\", \"Clear\", %i, \"%s\", %i, %i, %i);";
		int commandlen;
		char *command;
		
		//Autogen ID from start pos
		itoa(input.Start, ID, 16);
		
		commandlen = snprintf(NULL, 0, str, ID,
			input.FileID, ModUUID, input.Start, input.End, len);
		command = malloc(commandlen);
		snprintf(command, commandlen, str, ID,
			input.FileID, ModUUID, input.Start, input.End, len);
			
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
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
int Mod_AddSpace(struct ModSpace input, int start, int len, char *ModUUID){
	//Compute the wanted Start address and End address
	//Tries to go as low as possible, but sometimes the ranges clash.
	int StartAddr = start > input.Start ? start : input.Start;
	int EndAddr = (StartAddr + len) < input.End ? (StartAddr + len) : input.End;
	
	char command[1024] = "";
	char temp[1024] = ""; // For concatenation
	
	if (strcmp(input.ID, "") == 0){
		itoa(input.Start, input.ID, 16);  //If no ID, make one.
	}
	//Create new AddSpc table row
	sprintf(command, "INSERT INTO Spaces "
		"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len') VALUES (\"%s\", \"Add\", %i, \"%s\", %i, %i, %i);",
		input.ID, input.FileID, ModUUID, StartAddr, EndAddr, len);
	
	//Create a ClearSpc entry from [input.start] to [StartAddr] if not equal
	if (input.Start != StartAddr){
		sprintf(temp, "INSERT INTO Spaces "
			"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len') VALUES (\"%s\".StartBuff, \"Clear\", %i, \"%s\", %i, %i, %i);",
			input.ID, input.FileID, ModUUID, input.Start, StartAddr, StartAddr - input.Start);
		strcat(command, temp);
	}
	
	//Create a ClearSpc entry from EndAddr to [input.end] if not equal
	if (input.End != EndAddr){
		sprintf(temp, "INSERT INTO Spaces "
			"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len') VALUES (\"%s\".EndBuff, \"Clear\", %i, \"%s\", %i, %i, %i);",
			input.ID, input.FileID, ModUUID, EndAddr, input.End, input.End - EndAddr);
		strcat(command, temp);
	}

	//Execute
	sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
	return 0;
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
		char *OldBytesRaw, *OldBytes, *command;
		int result;
		
		//Read the old bytes
		OldBytesRaw = calloc(len, sizeof(char));
		result = read(handle, OldBytesRaw, len);
		//Quick error checking
		if (result != len){return FALSE;}
		//Convert to hex string
		OldBytes = Bytes2Hex(OldBytesRaw, len);
		free(OldBytesRaw);
		
		//Construct SQL Statement
		{
			char temp[32] = "";
			int temp2 = len + 100 + strlen(FreeSpace.ID);
			itoa(temp2, temp, 10);
			//AlertMsg(temp, "Allocation size.");
		}
		
		command = malloc((len*2) + 100 + strlen(FreeSpace.ID));
		sprintf(command, 
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
	ByteStr = strdup(JSON_GetStr(patchCurr, "Value"));
	ByteMode = strdup(JSON_GetStr(patchCurr, "AddType"));
	
	//Error checking
	if (strcmp(ByteMode, "") == 0){
		char *Mode;
		Mode = strdup(JSON_GetStr(patchCurr, "Mode"));
		
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
 *                Patches support 5 "operations":
 *                	- ADD: Adds given bytes wherever room is found in given range.
 *                	- CLEAR: Marks given space as clear. Does not effect file.
 *                	- REPL: Marks given range as clear and adds given bytes in place.
 *                	- MOVE: Marks given range 1 as clear and adds given bytes 
 *                	        to given range 2.
 *                	- COPY: Adds given bytes from range 1 to range 2.
 *                If no range is specified, the functions act on the whole file:
 *                	- ADD: Copies file from mod's folder to game's folder
 *                	- CLEAR: Removes file from game's folder
 *                	- REPL: Removes file from game's folder and adds file from
 *                	        mod's folder in place.
 *                	- MOVE: Moves file from one position in game's dir to another
 *                	- COPY: Copies file from one position in game's dir to another
 * =====================================================================================
 */
BOOL Mod_Install(json_t *root)
{
	json_t *patchArray, *patchCurr;
	size_t i;
	char ModUUID[128] = "";
	strcpy(ModUUID, JSON_GetStr(root, "UUID"));
	
	//AlertMsg("Installing mod...", "");
	
	patchArray = json_object_get(root, "patches");
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
			char *errormsg;
			int errorlen;
			char str[] = "This mod appears to have an issue. "
			           "Installation cannot continue.\n\n"
			           "Please contact the mod developer with "
			           "the following information:\n"
			           "- The Value parameter is missing on patch no. %i.";
				   
			errorlen = snprintf(NULL, 0, str, i);
			errormsg = malloc(errorlen);
			snprintf(errormsg, errorlen, str, i);
			
			AlertMsg(errormsg, "JSON Error");
			free(errormsg);
			return FALSE;
		}

		input.Start = JSON_GetInt(patchCurr, "Start");
		input.End = JSON_GetInt(patchCurr, "End") + 1;
		input.FileID = GetFileID(JSON_GetStr(patchCurr, "File"));
		//We might write to input.ID, so clear it out for later.
		strcpy(input.ID, "");
		
		//Check if Start is a UUID
		if (!json_is_integer(json_object_get(patchCurr, "Start"))){
			//If it is, replace the info with the start/end of the given UUID
			char *command = NULL, *UUID = NULL;
			json_t *out, *row;
			int commandlen;
			char str[] = "SELECT Start, End FROM Spaces "
			             "WHERE ID = '%s' AND UsedBy IS NULL;";
			UUID = strdup(JSON_GetStr(patchCurr, "Start"));
			
			//Get the list
			commandlen = strlen(str) + strlen(UUID);
			command = malloc(commandlen);
			snprintf(command, commandlen, str, UUID);

			out = SQL_GetJSON(command);
			row = json_array_get(out, 0);
			free(command);
			
			//Make sure list is not empty
			if(json_array_size(out) == 0){
				sprintf(command, "Given UUID %s in patch no. %i was not found.", UUID, i);
				AlertMsg(command, "UUID not found");
				return FALSE;
			}
			
			//Parse the list
			input.Start = JSON_GetInt(row, "Start");
			input.End = JSON_GetInt(row, "End");
			
			//If we're doing a replacement, make the new patch
			//take the UUID of the old space.
			if(strcmp(Mode, "Repl") == 0){
				char *ByteStr = strdup(JSON_GetStr(patchCurr, "Value"));
				strcpy(input.ID, ByteStr);
			}
			
			free(UUID);
			json_decref(row);
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
			ModOp_Clear(input, ModUUID);
			if (ModOp_Add(input, file, bytes, len, ModUUID, FALSE) == FALSE){return FALSE;}
		}
		else if(strcmp(Mode, "Clear") == 0){
			ModOp_Clear(input, ModUUID);
		}
		else if(strcmp(Mode, "Add") == 0){
			if (ModOp_Add(input, file, bytes, len, ModUUID, TRUE) == FALSE){return FALSE;}
		}
		//Will add Copy and Move later. Low-priority.
		//Move is higher-priority because that's awesome for finding space
		else{
			AlertMsg("Unknown patch mode", Mode);
			return FALSE;
		}
		
		close(file);
	}

	//Add mod to database
	{
		//char command[4096] = "";
		char *command = NULL;
		int commandlen;
		char str[] = "INSERT INTO Mods "
			"('UUID', 'Name', 'Desc', 'Auth', 'Ver', 'Date', 'Cat') VALUES "
			"(\"%s\", \"%s\", \"%s\", \"%s\",  %i,   \"%s\", \"%s\");";
		commandlen = snprintf(NULL, 0, str,
			JSON_GetStr(root, "UUID"), JSON_GetStr(root, "Name"),
			JSON_GetStr(root, "Desc"), JSON_GetStr(root, "Auth"), 
			JSON_GetInt(root, "Ver"), JSON_GetStr(root, "Date"),
			JSON_GetStr(root, "Cat")
		);
		command = malloc(commandlen);
		snprintf(command, commandlen, str,
			JSON_GetStr(root, "UUID"), JSON_GetStr(root, "Name"),
			JSON_GetStr(root, "Desc"), JSON_GetStr(root, "Auth"), 
			JSON_GetInt(root, "Ver"), JSON_GetStr(root, "Date"),
			JSON_GetStr(root, "Cat")
		);
		sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
		free(command);
	}
		
	return TRUE;
}

BOOL Mod_FindDep(char *ModUUID)
{
	char *command;
	int modCount, commandlen;
	BOOL result = FALSE;
	char str[] = "SELECT COUNT(*) FROM Dependencies "
		"JOIN Mods ON Dependencies.ParentUUID = Mods.UUID "
		"WHERE Dependencies.ChildUUID = '%s';";
	
	commandlen = snprintf(NULL, 0, str, ModUUID);
	command = malloc(commandlen);
	snprintf(command, commandlen, str, ModUUID);
	
	modCount = SQL_GetNum(command);
	free(command);
	//AlertMsg(command, "SQL Output");

	if (modCount != 0){
		result = TRUE;
	}
	return result;
}

void Mod_FindDepAlert(char *ModUUID){
	//char errormsg[4096] = "Uninstalling this mod will require "
	//                      "you to uninstall the following:\n\n";
	char *errormsg, *command;
	int i, commandlen, errorlen = 128;
	json_t *out, *row;
	errormsg = malloc(errorlen);
	strcpy(errormsg, "Uninstalling this mod will require "
	                 "you to uninstall the following:\n\n");
			 
	commandlen = snprintf(NULL, 0,
		"SELECT Name, Auth, Ver FROM Dependencies "
		"JOIN Mods ON Dependencies.ParentUUID = Mods.UUID "
		"WHERE Dependencies.ChildUUID = '%s';",
		ModUUID
	);
	command = malloc(commandlen);
	snprintf(command, commandlen,
		"SELECT Name, Auth, Ver FROM Dependencies "
		"JOIN Mods ON Dependencies.ParentUUID = Mods.UUID "
		"WHERE Dependencies.ChildUUID = '%s';",
		ModUUID
	);
	out = SQL_GetJSON(command);
	free(command);
	
	json_array_foreach(out, i, row){
		char *temp;
		int templen = 0;
		templen += JSON_GetStrLen(row, "Name");
		templen += JSON_GetStrLen(row, "Ver");
		templen += JSON_GetStrLen(row, "Auth");
		templen += strlen("- version by \n");
		
		while(strlen(errormsg) + templen > errorlen){
			errorlen *= 2; errormsg = realloc(errormsg, errorlen);
		}
		
		temp = malloc(templen);

		//Print it into a string
		snprintf(temp, templen, "- %s version %s by %s\n",
			JSON_GetStr(row, "Name"),
			JSON_GetStr(row, "Ver"),
			JSON_GetStr(row, "Auth")
		);
		
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
		char *filename = GetFileName(JSON_GetInt(row, "File"));
		filehandle = File_OpenSafe(filename, _O_BINARY|_O_RDWR);
		free(filename);
	}
	//Get offset & length
	offset = JSON_GetInt(row, "Start"); 
	datalen = JSON_GetInt(row, "Len");
	
	ByteStr = malloc(datalen*2);
	assert(ByteStr != NULL);
	
	//Retrieve data
	{
		char str[] = "SELECT OldBytes FROM Revert WHERE PatchUUID = '%s';";
		char *PatchUUID = strdup(JSON_GetStr(row, "ID"));
		int commandlen = strlen(str) + strlen(PatchUUID);
		char *command = malloc(commandlen);
		snprintf(command, commandlen, str, PatchUUID);
		ByteStr = strdup(SQL_GetStr(command));
		free(command);
	}

	//Convert data string from hex to raw bytes
	bytes = Hex2Bytes(ByteStr, datalen);
	
	//Write back data
	//File_WriteBytes(filehandle, offset, bytes, datalen);
	
	//Remove the space and the old bytes
	ModOp_UnClear(row); //Efficiency!
	{
		char str[] = "DELETE FROM Revert WHERE PatchUUID = '%s';";
		char *PatchUUID = strdup(JSON_GetStr(row, "ID"));
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

//Just remove the space. (We are doing this in order for a reason.)
void ModOp_UnClear(json_t *row)
{
	char str[] = "DELETE FROM Spaces WHERE ID = '%s';";
	char *PatchUUID = strdup(JSON_GetStr(row, "ID"));
	int commandlen = strlen(str) + strlen(PatchUUID);
	char *command = malloc(commandlen);
	snprintf(command, commandlen, str, PatchUUID);
	
	sqlite3_exec(CURRDB, command, NULL, NULL, NULL);
	
	free(command);
	free(PatchUUID);
	return;
}

BOOL Mod_Uninstall(char *ModUUID)
{
	int modCount, i, modStop;
	
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
		char str[] = "SELECT RowID FROM Mods WHERE UUID = '%s';";
		int commandlen;
		commandlen = strlen(str) + strlen(ModUUID);
		command = malloc(commandlen);
		snprintf(command, commandlen, str, ModUUID);
		
		modStop = SQL_GetNum(command);
		free(command);
	}
	
	//Perform operations in reverse
	for (i = modCount; i >= modStop; i--){
		//Get list of operations
		char *command = NULL;
		json_t *out, *row;
		int commandlen, j;
		char str[] = "SELECT Spaces.* FROM Spaces "
			"JOIN Mods ON Spaces.Mod = Mods.UUID "
			"WHERE Mods.RowID = %i;";
		
		commandlen = snprintf(NULL, 0, str, i);
		command = malloc(commandlen);
		snprintf(command, commandlen, str, i);
		
		out = SQL_GetJSON(command);
		free(command);
		
		//Loop through operations
		json_array_foreach(out, j, row){
			//Find type
			char *SpaceType = strdup(JSON_GetStr(row, "Type"));
			
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

	}
	//Reinstall existing mods
	return TRUE;
}

///Win32 GUI components
//////////////////////////////

// Patch Viewer Dialog Procedure
BOOL CALLBACK Dlg_Patch(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
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
}

// About Dialog Procedure
BOOL CALLBACK Dlg_About(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
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
	return;
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
	
		commandlen = snprintf(NULL, 0, str,
			JSON_GetStr(spaceCurr, "Label"), 0, "",
			start, end, end-start
		);
		command = malloc(commandlen);
		snprintf(command, commandlen, str,
			JSON_GetStr(spaceCurr, "Label"), 0, "",
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
		do {
			strcpy(GAMEPATH, SelectInstallFolder(GameCfg));
			if(strcmp(GAMEPATH, "QUITMSG") == 0){PostQuitMessage(0); break;}
		} while (strcmp(GAMEPATH, "") == 0);
		
		// Init the list of File IDs
		//FileIDCount = 1;
		//FileIDList = calloc(FileIDCount, sizeof(struct FileID));
		//FileIDList[0].ID = 0;
		//strcpy (FileIDList[0].path, JSON_GetStr(GameCfg, "GameEXE")); //Because strings are special

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
				MAKEINTRESOURCE(IDD_ABOUT), hwnd, Dlg_About);
		break;
		case IDC_MAINEXEMORE:
			// Call Patch dialog with argument set to file ID 0 (the .EXE)
			DialogBoxParam(GetModuleHandle(NULL),
				MAKEINTRESOURCE(IDD_PATCHES), hwnd, Dlg_Patch, 0);
		break;
		case IDC_MAINDISKMORE:
			DialogBox(GetModuleHandle(NULL),
				MAKEINTRESOURCE(IDD_DISK), hwnd, Dlg_About);
		break;
		case IDLOAD:{
			json_t *jsonroot;
				
			//Get name of mod to load
			char fpath[MAX_PATH] = ""; // Path mod is located at
			char fname[MAX_PATH] = ""; // Filename of mod
	
			SelectFile(hwnd, &fpath, &fname, "Mod Metadata (.json)\0*.json\0\0");
			//If no path given, quit
			if(strcmp(fpath, "") == 0){return TRUE;}
	
			/* Copy mod to [path]/mods/[name]
			// Set destination
			//strcpy(modloc, path);
			//strcpy(modloc, "//mods//");
			//strcpy(modloc, fname);

			// Actually copy
			// There is no POSIX copy function, so we're using the Windows API.*/
			
			///Load
			jsonroot = JSON_Load(&fpath);
			if (jsonroot == NULL){return TRUE;}
			
			///Verify
			if (Mod_CheckCompat(jsonroot) == FALSE){return TRUE;}
			if (Mod_CheckConflict(jsonroot) != 0){return TRUE;}
			if (Mod_CheckDep(jsonroot) == FALSE){return TRUE;}
			
			///Install mod
			//Disable window during install
			EnableWholeWindow(hwnd, FALSE);
			
			if (Mod_Install(jsonroot) == FALSE){
				//Restore backup
			};
			
			///Update mod list
			UpdateModList(GetDlgItem(hwnd, IDC_MAINMODLIST));
			UpdateModDetails(hwnd, SendMessage(
					GetDlgItem(hwnd, IDC_MAINMODLIST),
					LB_GETCURSEL, 0, 0)
				);
			
			//Re-enable window
			EnableWholeWindow(hwnd, TRUE);
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
				ModUUID = strdup(SQL_GetStr(command));
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
