/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "includes.h"              // LOCAL: All includes
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages
#include "interface/win32/interface.h"

//TEMPORARY
#define WINVER 0x0400              // Windows 95/NT4+ (common)
#define _WIN32_WINNT 0x0400        // Windows NT4+
#define _WIN32_IE 0x0000           // No IE requirement
#include <windows.h>               // Win32 APIs for the frontend
#include <commctrl.h>              // Common Controls for listbox, etc.
#include <Shlobj.h>                // "Open Folder" dialog
#include "interface/win32/resource.h"

//Globals
struct ProgConfig CONFIG = {0};    //Global program configuration

const long PROGVERSION =           // Program version in format 0x00MMmmbb
	PROGVERSION_BUGFIX + 
	(PROGVERSION_MINOR*0x100) +
	(PROGVERSION_MAJOR*0x10000);
//Name of program's primary author and website
const char PROGAUTHOR[] = "InvisibleUp";
const char PROGSITE[] = "http://github.com/InvisibleUp/SrModLdr/";

extern sqlite3 *CURRDB = NULL;                   //Current database holding patches
enum errCode CURRERROR = errNOERR;
HINSTANCE CURRINSTANCE = NULL;

///Win32 helper functions
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
	if(CURRERROR != errNOERR){return FALSE;}
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(
			CURRDB, query, strlen(query)+1,
			&command, NULL
		)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_int(command, 1, File) |
		sqlite3_bind_text(command, 2, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 3, "Add", -1, SQLITE_STATIC)
	) != 0){return -1;}
	
	add = SQL_GetNum(command);
	if(CURRERROR != errNOERR){return -1;}
	
	if(SQL_HandleErrors(__LINE__, sqlite3_reset(command)) != 0){
		CURRERROR = errCRIT_DBASE; return -1;
	}
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 3, "Clear", -1, SQLITE_STATIC)
	   ) != 0){return -1;}
	   
	clear = SQL_GetNum(command);
	if(CURRERROR != errNOERR){return -1;}
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
		result = NULL;
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
int SQL_HandleErrors(int lineno, int SQLResult)
{
	char *message = NULL;
	
	if(
		SQLResult == SQLITE_OK ||
		SQLResult == SQLITE_DONE ||
		SQLResult == SQLITE_ROW
	){
		return 0;
	}

	asprintf(&message,
		"Internal Database Error!\n"
		"Error code %d\n"
		"Line %d\n"
		, SQLResult, lineno);
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
			"An error has occurred loading %s.\n\n"
			//"Contact the mod developer with the following info:\n"
			"line %d:%d - %s", fpath, error.line,
			error.column, error.text);
		AlertMsg (buf, "Invalid JSON");
		safe_free(buf);
		return NULL;
	}
	if(!json_is_object(root)){
		AlertMsg ("An error has occurred loading the file.\n\n"
			//"Contact the mod developer with the following info:\n"
			"The root element of the JSON is not an object.",
			"Invalid JSON");
		json_decref(root);
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
	int mode = ReadOnly ? (F_OK) : (F_OK | W_OK);
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
 *  Description:  Tests if a file is found in the given whitelist by testing
 *                filesize and checksum. Returns index of version in whitelist.
 * =====================================================================================
 */
int File_Known(const char *FileName, json_t *whitelist)
{
	unsigned long fchecksum;
	signed long fsize;
	size_t i;
	json_t *value;
	CURRERROR = errNOERR;
	
	chdir(CONFIG.CURRDIR); //Change directory (!)

	//Get filesize
	fsize = filesize(FileName);
	if(fsize == -1){
		CURRERROR = errWNG_BADFILE;
		return -1;
	}
	
	//Get the checksum of the file
	fchecksum = crc32File(FileName);
	if(fchecksum == 0){
		CURRERROR = errWNG_BADFILE;
		return -1;
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
		
		if (fsize == Size && fchecksum == ChkSum){return i;}
	}
	
	CURRERROR = errWNG_BADFILE;
	return -1;
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
/*void File_TransactWrite()
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
}*/

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
	for(i = 0; i < len; i++){
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
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT)
		) != 0){safe_free(UUID);
			CURRERROR = errCRIT_DBASE; return ERR;}
		   
		oldver = SQL_GetNum(command);
		//Check that GetNum succeeded
		if(CURRERROR != errNOERR){return ERROR;}
		if(oldver == -1){FirstInstall = TRUE;}
		
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
			
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT) |
				sqlite3_bind_int(command, 2, ver)
			) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
			   
			ModCount = SQL_GetNum(command);
			if(CURRERROR != errNOERR){return FALSE;}
			
			if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
				CURRERROR = errCRIT_DBASE; return FALSE;
			}
			safe_free(UUID);
		}
		
		if (ModCount == 0){
			//There are missing mods. Roll back and abort.
			sqlite3_stmt *command;
			const char *query = "DELETE FROM Dependencies WHERE ParentUUID = ?;";
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
				sqlite3_bind_text(command, 1, ParentUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
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
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT) |
				sqlite3_bind_text(command, 2, ParentUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
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
			
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
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
			
			if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, FileName, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return -1;}
	
	ID = SQL_GetNum(command);
	
	if(CURRERROR != errNOERR){return -1;}
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_int(command, 1, input)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return "";
	}
	
	output = SQL_GetStr(command);
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0){CURRERROR = errCRIT_DBASE; return -1;}
	   
	IDCount = SQL_GetNum(command);
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return -1;
	}
	//Check that GetNum succeeded
	if(IDCount == -1 || CURRERROR != errNOERR){return -1;}
	
	//New ID is highest ID + 1
	ID = IDCount + 1;
	
	//Insert new ID into database
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_int(command, 1, ID) |
		sqlite3_bind_text(command, 2, FileName, -1, SQLITE_TRANSIENT)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
const char * Var_GetType_Str(enum VarType type)
{
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
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
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
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
	
	json_decref(VarArr);
	return result;
}

//Converts JSON to variable value
struct VarValue Var_GetValue_JSON(json_t *VarObj, const char *ModUUID)
{
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
	result.mod = strdup(ModUUID);
	result.persist = JSON_GetInt(VarObj, "Persist") ? TRUE : FALSE;
	
	safe_free(value);
	return result;
}

//Create Public Variable Listbox entires in SQL database
void Var_CreatePubList(json_t *PubList, const char *UUID)
{
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
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_int(command, 2, num) |
			sqlite3_bind_text(command, 3, label, -1, SQLITE_TRANSIENT)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
			safe_free(label);
			CURRERROR = errCRIT_DBASE;
			return;
		}
		command = NULL;
		safe_free(label);
	}	
}

//Copy Public Variable Listbox entires to new ListBox (Win32)
HWND Var_CreateListBox(HWND hParent, const char *VarUUID)
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
			sqlite3_bind_text(command, 1, VarUUID, -1, SQLITE_TRANSIENT)
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

BOOL Var_ClearEntry(const char *ModUUID)
{
	//Remove ListBoxes
	{
		sqlite3_stmt *command;
		const char *query = "DELETE FROM VarList WHERE Var IN "
		                    "(SELECT Var FROM VarList "
	                            "JOIN Variables ON VarList.Var = Variables.UUID "
	                            "WHERE Variables.Persist = 0 AND "
				    "Variables.Mod = ?)";
		CURRERROR = errNOERR;
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

	}
	
	//Remove variables
	{
		sqlite3_stmt *command;
		const char *query = "DELETE FROM Variables WHERE Mod = ? AND Persist = 0";
		CURRERROR = errNOERR;
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
	}
	
	return TRUE;
}


BOOL Var_UpdateEntry(struct VarValue result)
{
	//Update existing variable instead of making a new one.	
	sqlite3_stmt *command;
	const char *query = "UPDATE Variables SET "
			    "'Value' = ? WHERE UUID = ?;";
	CURRERROR = errNOERR;
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
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
	
	if(SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}
	command = NULL;
	return TRUE;
}

BOOL Var_MakeEntry(struct VarValue result)
{
	CURRERROR = errNOERR;
	//Check if variable already exists
	if(Var_GetValue_SQL(result.UUID).type != INVALID){
		return Var_UpdateEntry(result);
	}	
		
	//Compose and execute an insert command.
	{	
		sqlite3_stmt *command;
		char *query = "INSERT INTO Variables "
		              "(UUID, Mod, Type, PublicType, Desc, Value, Persist) "
			      "VALUES (?, ?, ?, ?, ?, ?, ?);";
		
		//Compose SQL
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, result.UUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 2, result.mod, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 3, Var_GetType_Str(result.type), -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 4, result.publicType, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 5, result.desc, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_int(command, 7, result.persist)
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
		if(SQL_HandleErrors(__LINE__, sqlite3_step(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
	}
		
	return TRUE;
}

BOOL Var_MakeEntry_JSON(json_t *VarObj, const char *ModUUID)
{
	struct VarValue result;
	BOOL retval = FALSE;
	CURRERROR = errNOERR;
	
	//Get variable value and type
	result = Var_GetValue_JSON(VarObj, ModUUID);
	
	//If PublicType == list, create and insert the PublicList
	//This may cause issues if you don't do this manually and call
	//Var_MakeEntry directly. Eh.
	if(strcmp(result.publicType, "List") == 0){
		json_t *PubList = json_object_get(VarObj, "PublicList");
		Var_CreatePubList(PubList, result.UUID);
	}

	//Insert via SQL
	retval = Var_MakeEntry(result);
	
	//Free junk in those variables
	safe_free(result.UUID);
	safe_free(result.desc);
	safe_free(result.publicType);
	safe_free(result.mod);
	
	return retval;
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
struct ModSpace Mod_FindSpace(struct ModSpace input, const char *ModUUID, BOOL Clear)
{
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
		                    "Len >= ? AND Start <= ? AND End >= ? "
		                    "AND File = ? ORDER BY Len;"; 
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1,
				Clear ? "Clear":"Add", -1, SQLITE_STATIC) ||
			sqlite3_bind_int(command, 2, input.Len) ||
			sqlite3_bind_int(command, 3, input.Start) ||
			sqlite3_bind_int(command, 4, input.End) ||
			sqlite3_bind_int(command, 5, input.FileID)
		) != 0){CURRERROR = errCRIT_DBASE; return result;}
		
		out = SQL_GetJSON(command);
		
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
		char *query2Arg = NULL;
		sqlite3_stmt *command;
		const char *query1 = "UPDATE Spaces SET UsedBy = ?, ID = ? "
		                     "WHERE ID = ? AND Type = ?";
		const char *query2 = "SELECT COUNT(*) FROM Spaces WHERE "
		                     "ID LIKE ?";
		int oldCount;

		//Is this scalable? I hope it is.
		//The answer is no.
		
		asprintf(&query2Arg, "%s~old%%", IDStr);
		
		//Get count of existing "%s~oldx" items
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, query2Arg, -1, SQLITE_TRANSIENT)
		) != 0){
			safe_free(query2Arg);
			CURRERROR = errCRIT_DBASE;
			return result;
		}
		oldCount = SQL_GetNum(command);
		sqlite3_finalize(command);
		
		asprintf(&NewID, "%s~old%d", IDStr, oldCount);
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT) ||
			sqlite3_bind_text(command, 2, NewID, -1, SQLITE_TRANSIENT) ||
			sqlite3_bind_text(command, 3, IDStr, -1, SQLITE_TRANSIENT) ||
			sqlite3_bind_text(command, 4, Clear ? "Clear":"Add", -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
		) != 0){
			safe_free(NewID);
			CURRERROR = errCRIT_DBASE;
			return result;
		}
		command = NULL;
		safe_free(query2Arg);
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
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_TRANSIENT)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return strdup("");
	}
	
	out = SQL_GetStr(command);
	
	if(CURRERROR != errNOERR){
		safe_free(out);
		return strdup("");
	}
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		safe_free(out);
		return strdup("");
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
	if(CURRERROR != errNOERR){return FALSE;}

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
	//I have no idea if this code even works, tbh.
	Occ = Mod_FindSpace(input, ModUUID, FALSE);
	if(CURRERROR != errNOERR){return FALSE;}
	
	if(Occ.Valid == FALSE){ //Nothing found
		OccOwner = NULL;
	} else { //Something found
		OccOwner = Mod_FindPatchOwner(Occ.ID);
	}
	if(CURRERROR != errNOERR){return FALSE;}
	
	//Check if space is occupied
	if(!OccOwner){OccOwner = strdup("");} //Just in case
	if(strcmp(OccOwner, "") != 0){
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

		/* Create ID if not existing */
		
		if (Occ.ID){
			input.ID = strdup(Occ.ID);
		} else if (strcmp(input.ID, "") == 0) { //Autogen ID from start pos
			safe_free(input.ID);
			asprintf(&input.ID, "%d-%X", input.FileID, input.Start); 
		}
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, input.ID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_int(command, 2, input.FileID) |
			sqlite3_bind_text(command, 3, ModUUID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_int(command, 4, input.Start) |
			sqlite3_bind_int(command, 5, input.End) |
			sqlite3_bind_int(command, 6, input.Len)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		command = NULL;
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
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	
	//Create new AddSpc table row
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, input.ID, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 2, "Add", -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 3, input.FileID) |
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_int(command, 5, StartAddr) |
		sqlite3_bind_int(command, 6, EndAddr) |
		sqlite3_bind_int(command, 7, input.Len)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_reset(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return FALSE;
	}
	
	//Create a ClearSpc entry from [input.start] to [StartAddr] if not equal
	if (input.Start != StartAddr){
		char *inputStartBuff = NULL;
		asprintf(&inputStartBuff, "%s.StartBuff", input.ID);
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, inputStartBuff, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 2, "Clear", -1, SQLITE_STATIC) |
			sqlite3_bind_int(command, 5, input.Start) |
			sqlite3_bind_int(command, 6, StartAddr) |
			sqlite3_bind_int(command, 7, StartAddr - input.Start)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_reset(command)
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
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, inputEndBuff, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 2, "Clear", -1, SQLITE_STATIC) |
			sqlite3_bind_int(command, 5, input.End) |
			sqlite3_bind_int(command, 6, EndAddr) |
			sqlite3_bind_int(command, 7, input.End - EndAddr)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_reset(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			safe_free(inputEndBuff);
			return FALSE;
		}
		safe_free(inputEndBuff);
	}
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
		
		lseek(handle, input.Start, SEEK_SET);
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
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, FreeSpace.ID, -1, SQLITE_TRANSIENT) |
			sqlite3_bind_text(command, 2, OldBytes, -1, SQLITE_TRANSIENT)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, UUID, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	
	out = SQL_GetJSON(command);
	row = json_array_get(out, 0);
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
	
	//Init progress dialog
	patchArray = json_object_get(root, "patches");
	ProgDialog = ProgDialog_Init(
		json_array_size(patchArray),
		"Installing Mod..."
	);
	if(CURRERROR != errNOERR){
		ProgDialog_Kill(ProgDialog);
		return FALSE;
	}
	
	//Add variable entries
	{
		json_t *varArray, *varCurr;
		varArray = json_object_get(root, "variables");

		//Add user-defined variables
		json_array_foreach(varArray, i, varCurr){
			struct VarValue tempVal;
			char *varName;
			
			//Check if variable exists
			varName = JSON_GetStr(varCurr, "UUID");
			tempVal = Var_GetValue_SQL(varName);
			if(tempVal.type == INVALID){
				//Variable does not exist; make it!
				Var_MakeEntry_JSON(varCurr, ModUUID);
			}
		}
		
		//Add modloader-defined variables
		{
			struct VarValue varCurr;
			int ModVer = JSON_GetInt(root, "Ver");
			
			//Active
			varCurr.type = uInt8;
			asprintf(&varCurr.UUID, "Active.%s", ModUUID); 
			varCurr.desc = strdup("Mod is installed.");
			varCurr.publicType = strdup("");
			varCurr.mod = strdup(ModUUID);
			varCurr.uInt32 = 1;
			varCurr.persist = FALSE;
			Var_MakeEntry(varCurr);
			
			//Version
			safe_free(varCurr.UUID);
			safe_free(varCurr.desc);
			varCurr.type = uInt32;
			varCurr.uInt32 = ModVer;
			asprintf(&varCurr.UUID, "Version.%s", ModUUID);
			varCurr.desc = strdup("Version of installed mod.");
			Var_MakeEntry(varCurr);
			
			//Cleanup
			safe_free(varCurr.UUID);
			safe_free(varCurr.desc);
			safe_free(varCurr.publicType);
			safe_free(varCurr.mod);
		}
	}
	
	//Install patches (if applicable)
	if(!patchArray || !json_is_array(patchArray)){
		goto Mod_Install_NextPatch;
	}
		
	json_array_foreach(patchArray, i, patchCurr){
		char *Mode = NULL;
		int file = -1;
		struct ModSpace input = {0};
		
		//Change directory back to game's root in case it was changed.
		chdir(CONFIG.CURRDIR);
		ErrNo2ErrCode();
		if(CURRERROR != errNOERR){
			retval = FALSE;
			goto Mod_Install_NextPatch;
		}
		
		//Open this patch's file
		file = File_OpenSafe(JSON_GetStr(patchCurr, "File"), _O_BINARY|_O_RDWR);
		if(file == -1){
			AlertMsg("`File` is not the path to a valid file.", "");
			retval = FALSE;
			goto Mod_Install_NextPatch;
		}
		
		//Get patch mode
		Mode = JSON_GetStr(patchCurr, "Mode");
		if(strcmp(Mode, "") == 0){
			//Mode was not found
			AlertMsg("`Mode` not defined for patch.", "JSON Error");
			retval = FALSE;
			goto Mod_Install_NextPatch;
		}
		
		//Fill input struct
		input = Mod_GetPatchInfo(patchCurr, file);
		if(input.Valid == FALSE){
			AlertMsg("GetPatchInfo failed...", "");
			retval = FALSE;
			goto Mod_Install_NextPatch;
		}
		
		//Check variable condition
		{
			char *CondVar = NULL;
			char *CondOp = NULL;
			
			struct VarValue CurrVar;
			BOOL CurrComp = FALSE;
			
			//Get variable settings
			CondVar = JSON_GetStr(patchCurr, "ConditionVar");
			CondOp = JSON_GetStr(patchCurr, "ConditionOp");
			
				
			//Retrieve variable 
			CurrVar = Var_GetValue_SQL(CondVar);
				
			//Compare variable
			switch(CurrVar.type){
			case Int32:
			case Int16:
			case Int8:
			{
				signed long CondVal;
				CondVal = JSON_GetInt(patchCurr, "ConditionValue");
				
				if(strcmp(CondOp, "EQUALS") == 0){
					CurrComp = (CurrVar.Int32 == CondVal);
				} else if(strcmp(CondOp, "NOT EQUALS") == 0){
					CurrComp = (CurrVar.Int32 != CondVal);
				} else if(strcmp(CondOp, "LESS") == 0){
					CurrComp = (CurrVar.Int32 < CondVal);
				} else if(strcmp(CondOp, "GREATER") == 0){
					CurrComp = (CurrVar.Int32 > CondVal);
				} else if(strcmp(CondOp, "LESS EQUAL") == 0){
					CurrComp = (CurrVar.Int32 <= CondVal);
				} else if(strcmp(CondOp, "GREATER EQUAL") == 0){
					CurrComp = (CurrVar.Int32 >= CondVal);
				} else {
					CurrComp = FALSE;
				}
			}
			break;
			
			case uInt32:
			case uInt16:
			case uInt8:{
				//This is going to be weird
				unsigned long CondVal;
				char *CondValStr;
				CondValStr = JSON_GetStr(patchCurr, "ConditionValue");
				CondVal = strtoul(CondValStr, NULL, 0);
				
				if(strcmp(CondOp, "EQUALS") == 0){
					CurrComp = (CurrVar.uInt32 == CondVal);
				} else if(strcmp(CondOp, "NOT EQUALS") == 0){
					CurrComp = (CurrVar.uInt32 != CondVal);
				} else if(strcmp(CondOp, "LESS") == 0){
					CurrComp = (CurrVar.uInt32 < CondVal);
				} else if(strcmp(CondOp, "GREATER") == 0){
					CurrComp = (CurrVar.uInt32 > CondVal);
				} else if(strcmp(CondOp, "LESS EQUAL") == 0){
					CurrComp = (CurrVar.uInt32 <= CondVal);
				} else if(strcmp(CondOp, "GREATER EQUAL") == 0){
					CurrComp = (CurrVar.uInt32 >= CondVal);
				} else {
					CurrComp = FALSE;
				}
				
				safe_free(CondValStr);
			}
			break;
			
			case IEEE32:
			case IEEE64:{
				double CondVal;
				CondVal = JSON_GetDouble(patchCurr, "ConditionValue");
				
				if(strcmp(CondOp, "EQUALS") == 0){
					CurrComp = (CurrVar.IEEE64 == CondVal);
				} else if(strcmp(CondOp, "NOT EQUALS") == 0){
					CurrComp = (CurrVar.IEEE64 != CondVal);
				} else if(strcmp(CondOp, "LESS") == 0){
					CurrComp = (CurrVar.IEEE64 < CondVal);
				} else if(strcmp(CondOp, "GREATER") == 0){
					CurrComp = (CurrVar.IEEE64 > CondVal);
				} else if(strcmp(CondOp, "LESS EQUAL") == 0){
					CurrComp = (CurrVar.IEEE64 <= CondVal);
				} else if(strcmp(CondOp, "GREATER EQUAL") == 0){
					CurrComp = (CurrVar.IEEE64 >= CondVal);
				} else {
					CurrComp = FALSE;
				}
			}
			break;
			
			}
				
			//Go to next mod if condition is false and valid
			if(
				!(strcmp(CondVar, "") == 0 ||
				CurrVar.type == INVALID) &&
				CurrComp == FALSE
			){
				safe_free(CondVar);
				safe_free(CondOp);
				goto Mod_Install_NextPatch;
			}
			safe_free(CondVar);
			safe_free(CondOp);
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
			if(retval == FALSE){
				goto Mod_Install_NextPatch;
			}
			
			//Add new one in new location
			retval = ModOp_Add(input, file, ModUUID); //Add to Dest

		//Unknown mode
		} else {
			AlertMsg("Unknown patch mode", Mode);
			retval = FALSE;
			goto Mod_Install_NextPatch;
		}
		
		if(retval == FALSE){
			goto Mod_Install_NextPatch;
		}

		//Create var for patch location
		{
			struct VarValue varCurr;
			
			//Active
			varCurr.type = uInt8;
			asprintf(&varCurr.UUID, "Start.%s.%s", ModUUID); 
			varCurr.desc = strdup("Start byte of patch adjusted for base address.");
			varCurr.publicType = strdup("");
			varCurr.mod = strdup(ModUUID);
			varCurr.persist = FALSE;

			//Adjust EXE offset into memory address
			//???
			varCurr.uInt32 = input.Start;

			Var_MakeEntry(varCurr);
			
			//Cleanup
			safe_free(varCurr.UUID);
			safe_free(varCurr.desc);
			safe_free(varCurr.publicType);
			safe_free(varCurr.mod);
		}
		
		//Install "Mini Patches"
		{
			json_t *miniArray, *miniCurr;
			size_t j;
			
			miniArray = json_object_get(patchCurr, "MiniPatches");
			if(!miniArray || !json_is_array(miniArray)){
				goto Mod_Install_NextPatch;
			}
			
			json_array_foreach(miniArray, j, miniCurr){
				char *CurrVarStr = NULL;		
				int CurrVarLen = 0;
				int CurrVarLoc = 0;
				struct VarValue CurrVar;
				struct ModSpace CurrInput;
				
				//Retrieve variable 
				CurrVarStr = JSON_GetStr(miniCurr, "Var");
				CurrVar = Var_GetValue_SQL(CurrVarStr);
				
				CurrVarLen = JSON_GetInt(miniCurr, "Len");
				CurrVarLoc = JSON_GetInt(miniCurr, "Pos");
				
				//Determine actual size
				switch(CurrVar.type){
				case IEEE64:
					CurrVarLen = min(CurrVarLen, 8);
				break;
				
				case Int32:
				case uInt32:
				case IEEE32:
					CurrVarLen = min(CurrVarLen, 4);
				break;
				
				case Int16:
				case uInt16:
					CurrVarLen = min(CurrVarLen, 2);
				break;
				
				case Int8:
				case uInt8:
					CurrVarLen = 1;
				break;
				
				default:
					goto MiniPatch_CleanUp;
				}
				
				if(CurrVarLen == 0){
					goto MiniPatch_CleanUp;
				}
				
				//Create ModSpace
				memcpy(&CurrInput, &input, sizeof(input)); 
				
				asprintf(&CurrInput.ID, "%s/%d", input.ID, j);
				
				CurrInput.Start += CurrVarLoc;
				CurrInput.End = CurrInput.Start + CurrVarLen;
				CurrInput.Len = CurrVarLen;
				
				CurrInput.Bytes = malloc(CurrVarLen);
				memcpy(&CurrInput.Bytes, &CurrVar.IEEE64 + (8 - CurrVarLen), CurrVarLen);
				
				CurrInput.Valid = TRUE;
				
				//Splice value in
				ModOp_Clear(CurrInput, ModUUID);
				ModOp_Add(CurrInput, file, ModUUID);

				safe_free(CurrInput.ID);
				
			MiniPatch_CleanUp:
				safe_free(CurrVarStr);
			}
		}
		
	Mod_Install_NextPatch:
		close(file);
		safe_free(input.ID);
		ProgDialog_Update(ProgDialog, 1);
		
		if(retval == FALSE || CURRERROR != errNOERR){
			goto Mod_Install_Cleanup;
		}
		
		//Yield control to other windows (Win32)
		#ifdef _WIN32
		{
			MSG message;
			if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)){
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
		}
		#endif
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
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, uuid, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 2, name, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 3, desc, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 4, auth, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_int(command, 5, ver) |
		sqlite3_bind_text(command, 6, date, -1, SQLITE_TRANSIENT) |
		sqlite3_bind_text(command, 7, cat, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	
	if(SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
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
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	
	modCount = SQL_GetNum(command);
	if(CURRERROR != errNOERR){return FALSE;}
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){CURRERROR = errCRIT_DBASE; return;}
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
	) != 0){CURRERROR = errCRIT_DBASE; return;}
	out = SQL_GetJSON(command);
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
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
		
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_TRANSIENT)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
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

	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_TRANSIENT)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
	
	chdir(CONFIG.CURRDIR); //In case path was changed during installation or w/e.
	
	//Check for dependencies
	if(Mod_FindDep(ModUUID) == TRUE){
		Mod_FindDepAlert(ModUUID);
		return FALSE;
	};
	
	//Count mods
	{
		sqlite3_stmt *command;
		const char *query = "SELECT COUNT(*) FROM Mods;";
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
		modCount = SQL_GetNum(command);
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
	}
	
	//Get stopping point (Uninstall everything up to selected mod)
	{
		sqlite3_stmt *command;
		const char *query = "SELECT RowID FROM Mods WHERE UUID = ?;";
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
		
		modStop = SQL_GetNum(command);
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
			
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
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
			
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
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
		
		//Get rid of "~oldx" at the end of spaces used by this mod
		{
			sqlite3_stmt *command;
			char *oldID;
			int oldCount;
			
			const char *query1 = "SELECT COUNT(*) FROM Spaces WHERE "
		                     "ID LIKE ?";
			const char *query2 = "UPDATE Spaces SET ID = REPLACE(ID, ?, '') "
			                     "WHERE UsedBy = ?;";
			
			//Get count of existing "%s~oldx" items
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
				sqlite3_bind_text(command, 1, CurrUUID, -1, SQLITE_TRANSIENT)
			) != 0){
				safe_free(CurrUUID);
				CURRERROR = errCRIT_DBASE;
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
			oldCount = SQL_GetNum(command);
			sqlite3_finalize(command);
			
			asprintf(&oldID, "%~old%d", CurrUUID, oldCount);
			
			//Remove any instances of "~old" in oldest ID
			
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__,
				sqlite3_bind_text(command, 1, oldID, -1, SQLITE_TRANSIENT) ||
				sqlite3_bind_text(command, 2, CurrUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
				sqlite3_bind_text(command, 1, CurrUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
				sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
				sqlite3_bind_text(command, 1, CurrUUID, -1, SQLITE_TRANSIENT)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
	
	if (Mod_Install(jsonroot) == FALSE){
		char *ModUUID = NULL;
		ErrCracker(CURRERROR); //Catch errors
		
		ModUUID = JSON_GetStr(jsonroot, "UUID");
		AlertMsg("Installation failed. Rolling back...", "Installation Failure");
		Mod_Uninstall(ModUUID);
		safe_free(ModUUID);
		
		retval = FALSE;
		goto Mod_InstallPrep_Cleanup;
	};
	
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

///Profile stuff
//Make a profile config file from profile data
void Profile_Save(
	const char *profile,
	const char *game,
	const char *gameConf,
	int checksum,
	const char *runpath
){
	json_t *Config = json_pack(
		"{s:s, s:s, s:i, s:s}",
		"GamePath", game,
		"MetadataPath", gameConf,
		"Checksum", checksum,
		"RunPath", runpath
	);
	json_dump_file(Config, profile, 0);
	json_decref(Config);
	return;
}

//Replaces SelectInstallFolder()
#ifdef _WIN32
char * Profile_FindMetadata(const char *gamePath)
{
	WIN32_FIND_DATA output;
	HANDLE hCfg;
	BOOL ResultCode;
	char *MetadataPath = NULL;
	
	//Set working directory to program directory
	chdir(CONFIG.PROGDIR);
	//For each metadata file in /games...
	hCfg = FindFirstFile(".\\games\\*.json", &output);
	if(hCfg == INVALID_HANDLE_VALUE){
		return NULL;
	} else {
		ResultCode = 1;
	}
	
	while(ResultCode != 0){
		json_t *CurrCfg, *whitelist;
		json_error_t error;
		char *GameEXE = NULL;

		//Compose directory name
		asprintf(&MetadataPath, "%s\\games\\%s", CONFIG.PROGDIR, output.cFileName);
		
		//Load "main file" name and whitelist
		CurrCfg = json_load_file(MetadataPath, 0, &error);
		if(CurrCfg == NULL){
			chdir(CONFIG.PROGDIR);
			ResultCode = FindNextFile(hCfg, &output);
			continue;
		}
		
		whitelist = json_object_get(CurrCfg, "Whitelist");
		if(whitelist == NULL){
			chdir(CONFIG.PROGDIR);
			ResultCode = FindNextFile(hCfg, &output);
			json_decref(CurrCfg);
			continue;
		}
		
		GameEXE = JSON_GetStr(CurrCfg, "GameEXE");

		//Set working directory to game directory
		chdir(gamePath);
		
		// Make sure it has the correct game file
		if(
			File_Exists(GameEXE, TRUE, FALSE) &&
			(File_Known(GameEXE, whitelist) != -1)
		){
			safe_free(GameEXE);
			json_decref(CurrCfg);
			break;
		}
		
		//Nope, not it. Load next file.
		safe_free(GameEXE);
		json_decref(CurrCfg);
		chdir(CONFIG.PROGDIR);
		ResultCode = FindNextFile(hCfg, &output);
	}
	if(GetLastError() == ERROR_NO_MORE_FILES){
		MetadataPath = NULL;
	}
	FindClose(hCfg);
	return MetadataPath;
}
#endif

//Dumps the current contents of global variables to CURRPROF
void Profile_DumpLocal(struct ProgConfig *LocalConfig)
{
	Profile_Save(
		LocalConfig->CURRPROF,
		LocalConfig->CURRDIR,
		LocalConfig->GAMECONFIG,
		LocalConfig->CHECKSUM,
		LocalConfig->RUNPATH
	);
	return;
}

struct ProgConfig * Profile_Load(char *fpath)
{
	struct ProgConfig *LocalConfig;
	json_t *GameCfg, *Profile;
	char *GameEXE;
	
	//Init struct
	LocalConfig = malloc(sizeof(struct ProgConfig));
	memset(LocalConfig, 0, sizeof(*LocalConfig));
	
	//Load profile
	Profile = JSON_Load(fpath);
	if(!Profile){
		json_decref(Profile);
		return LocalConfig;
	}
	
	//Set variables
	LocalConfig->CURRPROF = strdup(fpath);
	LocalConfig->GAMECONFIG = JSON_GetStr(Profile, "MetadataPath");
	LocalConfig->CURRDIR = JSON_GetStr(Profile, "GamePath");
	LocalConfig->RUNPATH = JSON_GetStr(Profile, "RunPath");
	LocalConfig->PROGDIR = strdup(CONFIG.PROGDIR);
	
	//Load game config
	chdir(CONFIG.PROGDIR);
	GameCfg = JSON_Load(LocalConfig->GAMECONFIG);
	if(!GameCfg){
		//Seems config is broken/missing
		json_decref(Profile);
		safe_free(LocalConfig->GAMECONFIG);
		safe_free(LocalConfig->CURRDIR);
		safe_free(LocalConfig->RUNPATH);
		return LocalConfig;
	}
	
	//Check if checksum is sane
	chdir(LocalConfig->CURRDIR);
	GameEXE = JSON_GetStr(GameCfg, "GameEXE");
	LocalConfig->CHECKSUM = crc32File(GameEXE);
	if(
		LocalConfig->CHECKSUM != JSON_GetInt(Profile, "Checksum") &&
		JSON_GetInt(Profile, "Checksum") != 0
	){
		//Bad checksum. Ugh.
		char *msg = NULL;
		asprintf(&msg, "%s has different contents compared\n"
		"to the last time this program was launched.\n\n"
		"You will need to manually restore your game backup\n"
		"and remove the '.db' file in your game's install folder.\n",
		GameEXE);
		safe_free(GameEXE);
		
		//Clear game path and checksum
		LocalConfig->CHECKSUM = 0; //This might be dangerous. Let's see.
		safe_free(LocalConfig->CURRDIR);
		chdir(CONFIG.PROGDIR);
	}
	
	//All good!
	json_decref(GameCfg);
	json_decref(Profile);
	return LocalConfig;
}


char *Profile_GetGameEXE(const char *cfgpath)
{
	char *GameEXE;
	json_t *CurrCfg;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return strdup("");
	}

	GameEXE = JSON_GetStr(CurrCfg, "GameEXE");
	json_decref(CurrCfg);
	return GameEXE;
}

char * Profile_GetGameVer(const char *cfgpath)
{
	char *GameVer, *GameEXE;
	json_t *CurrCfg, *whitelist;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return strdup("");
	}

	whitelist = json_object_get(CurrCfg, "Whitelist");
	if(whitelist == NULL){
		json_decref(CurrCfg);
		return strdup("");
	}

	//Could call funct, but really this is simpler.
	GameEXE = JSON_GetStr(CurrCfg, "GameEXE");

	GameVer = JSON_GetStr(
		json_array_get(
			whitelist,
			File_Known(GameEXE, whitelist)
		), "Desc"
	);
	json_decref(CurrCfg);
	return GameVer;
}

char * Profile_GetGameName(const char *cfgpath)
{
	char *GameName;
	json_t *CurrCfg;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return strdup("");
	}
	
	GameName = JSON_GetStr(CurrCfg, "GameName");
	
	json_decref(CurrCfg);	
	return GameName;
}

void Profile_EmptyStruct(struct ProgConfig *LocalConfig)
{
	safe_free(LocalConfig->CURRDIR);
	safe_free(LocalConfig->PROGDIR);
	safe_free(LocalConfig->CURRPROF);
	safe_free(LocalConfig->GAMECONFIG);
	safe_free(LocalConfig->RUNPATH);
	return;
}

///Win32 GUI components
//////////////////////////////

// Patch Viewer Dialog Procedure
// My god this code is ancient.
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

//Open database & create tables
BOOL SQL_Load(){
	CURRERROR = errNOERR;
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_open("mods.db", &CURRDB)
	) != 0 || SQL_HandleErrors(__LINE__, 
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
			"`Value`      BLOB,"
			"`Persist`    INTEGER);",
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
	
	//Add mod loader version var
	{
		struct VarValue varCurr;
		
		//Check if variable exists
		varCurr = Var_GetValue_SQL("Version.MODLOADER@invisibleup");
		if(varCurr.type == INVALID){
			//Variable does not exist; make it!
			varCurr.type = uInt32;
			varCurr.UUID = strdup("Version.MODLOADER@invisibleup");
			varCurr.desc = strdup("Version of mod loader (InvisibleUp's fork)");
			varCurr.publicType = strdup("");
			varCurr.mod = strdup("");
			varCurr.uInt32 = PROGVERSION;
			varCurr.persist = TRUE;
			
			Var_MakeEntry(varCurr);
			
		} else {
			//Update version number (just in case.)
			varCurr.uInt32 = PROGVERSION;
			Var_UpdateEntry(varCurr);
		}
		
		//Free everything.
		safe_free(varCurr.UUID);
		safe_free(varCurr.desc);
		safe_free(varCurr.publicType);
		safe_free(varCurr.mod);
	}
	
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
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		
		spaceCount = SQL_GetNum(command);
		if(CURRERROR != errNOERR){return FALSE;}
		
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		command = NULL;
	}
	
	//Actually return if there's something
	if (spaceCount != 0){return TRUE;}
	
	//To decrease disk I/O and increase speed
	if(SQL_HandleErrors(__LINE__, 
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
			
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, 
				sqlite3_bind_text(command, 1, JSON_GetStr(spaceCurr, "UUID"), -1, SQLITE_TRANSIENT) |
				sqlite3_bind_int(command, 2, start) |
				sqlite3_bind_int(command, 3, end) |
				sqlite3_bind_int(command, 4, end-start)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
			) != 0){
				CURRERROR = errCRIT_DBASE;
				return FALSE;
			}
			command = NULL;
		}
		{
			const char *query = "DELETE FROM Mods WHERE RowID = ?;";
			sqlite3_stmt *command;
			
			if(SQL_HandleErrors(__LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_bind_int(command, 1, i)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
			) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
			) != 0){
				CURRERROR = errCRIT_DBASE;
				return FALSE;
			}
			command = NULL;
		}
	}

	if(SQL_HandleErrors(__LINE__, 
		sqlite3_exec(CURRDB, "COMMIT TRANSACTION", NULL, NULL, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	return TRUE;
}


int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInst, char * cmdParam, int cmdShow)
{
	json_t *GameCfg;
	int result;
	
	//Init globals
	CURRINSTANCE = hInst; //Win32
	memset(&CONFIG, 0, sizeof(CONFIG));
	
	InitInterface();
	
	//Save current directory
	CONFIG.PROGDIR = malloc(MAX_PATH);
	if(!CONFIG.PROGDIR){
		CURRERROR = errCRIT_MALLOC;
		ErrCracker(CURRERROR);
	}
	getcwd(CONFIG.PROGDIR, MAX_PATH);
	
	//Load profile if needed
	if(!CONFIG.CURRPROF){
		EditConfig();
		if(
			CURRERROR == errUSR_ABORT ||
			!CONFIG.CURRDIR || strcmp(CONFIG.CURRDIR, "") == 0
		){
			//User wants to quit
			return 0;
		}
	}
	
	// Load program preferences/locations
	GameCfg = JSON_Load(CONFIG.GAMECONFIG);
	
	//Load SQL stuff
	SQL_Load();
	ErrCracker(CURRERROR);
	SQL_Populate(GameCfg);
	ErrCracker(CURRERROR);
	json_decref(GameCfg);
	
	result = ViewModList();

	//Exit
	sqlite3_close(CURRDB);
	return result;
}