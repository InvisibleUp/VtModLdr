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
struct ProgConfig CONFIG = {0};    // Global program configuration

const long PROGVERSION =           // Program version in format 0x00MMmmbb
	PROGVERSION_BUGFIX + 
	(PROGVERSION_MINOR*0x100) +
	(PROGVERSION_MAJOR*0x10000);
//Name of program's primary author and website
const char PROGAUTHOR[] = "InvisibleUp";
const char PROGSITE[] = "http://github.com/InvisibleUp/SrModLdr/";

extern sqlite3 *CURRDB = NULL;    // Current database holding patches
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
 *         Name:  GetUsedSpaceBytes
 *  Description:  Returns number of bytes used by a mod in the given file.
 * =====================================================================================
 */
int GetUsedSpaceBytes(const char *ModUUID, int File)
{
	int add, clear;
	sqlite3_stmt *command = NULL;
	const char *query = "SELECT SUM(Len) FROM Spaces WHERE "
	                    "File = ? AND Mod = ? AND Type = ?;";
	CURRERROR = errNOERR;
	
	//Get add spaces
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_int(command, 1, File) |
		sqlite3_bind_text(command, 2, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 3, "Add", -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return -1;
	}
	add = SQL_GetNum(command);
	if(CURRERROR != errNOERR){
		return -1;
	}
	
	//Subtract clear spaces
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_reset(command)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 3, "Clear", -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return -1;
	}
	clear = SQL_GetNum(command);
	if(CURRERROR != errNOERR){
		return -1;
	}
	
	//Return
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		return -1;
	}
	command = NULL;
	return (add - clear);
}

/*int GetDiskSpaceBytes(const char *ModUUID)
{
	int ModCount, i;
	sqlite3_stmt *command = NULL;
	const char *query = "SELECT Max(ROWID) FROM Spaces;";
	
	//Get number of mods
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return -1;
	}
	ModCount = SQL_GetNum(command);
	if(CURRERROR != errNOERR){
		return -1;
	}
	
	//Get disk space for every mod except 0 (:memory:) and ???
}*/

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
				//AlertMsg(coltext, "DEBUG coltext");
				value = json_string(coltext);
			}
			
			if(value == NULL){
				CURRERROR = errCRIT_FUNCT;
				AlertMsg("There was an error fetching data from\n"
					"the mod database. (Possible memory corruption!)\n"
					"The application will now quit to prevent data\n"
					"corruption. PLEASE contact the developer!",
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

	{
		//DEBUG: Dump JSON text
		//char *msg = json_dumps(result, JSON_INDENT(4) | JSON_ENSURE_ASCII);
	//	AlertMsg(msg, "DEBUG");

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
 *         Name:  SQL_GetBlob
 *  Description:  Returns the first item from an SQL query as raw data (len is output)
 *                Never used. That's probably a good thing.
 * =====================================================================================
 */
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
		signed long result = strtol(temp, NULL, 0);
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
 *         Name:  JSON_GetDouble
 *  Description:  Gives a double given a JSON tree and the name of a tag
 * =====================================================================================
 */
double JSON_GetDouble(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL) {return 0;}
	if (json_is_string(object)) {
		char *temp = JSON_GetStr(root, name);
		double result = strtod(temp, NULL);
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
 *                and sets CURRERROR if it is not. Also optionally warns
 *                if file is read-only.
 * =====================================================================================
 */
BOOL File_Exists(const char *file, BOOL InFolder, BOOL ReadOnly)
{
	int mode = ReadOnly ? (R_OK) : (R_OK | W_OK);
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
		accessflag = W_OK | R_OK;
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
 *                Simple wrapper around Windows's CopyFile function.
 *                Might be more complicated on POSIX systems.
 * =====================================================================================
 */
void File_Copy(const char *OldPath, const char *NewPath)
{
	CopyFile(OldPath, NewPath, FALSE);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Delete
 *  Description:  Delete [Path].
 *                Simple wrapper around Windows's DeleteFile function.
 *                Might be more complicated on POSIX systems.
 * =====================================================================================
 */
void File_Delete(const char *Path)
{
	DeleteFile(Path);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Deltree
 *  Description:  Deletes a directory and all files and folders within.
 *                If using POSIX libraries, it uses the function File_Deltree_CB()
 *                as a callback function for ntfw().
 *                If using Win32, it uses SHFileOperation. Input path must have
 *                terminating backslash.
 * =====================================================================================
 */
 
BOOL File_DelTree(char *DirPath)
{
	SHFILEOPSTRUCT SHStruct = {0};
	char *DirPathZZ = NULL;
	int retval = 0;
	//asprintf(&DirPathZZ, "%s\0", DirPath);
	//My only possible guess
	DirPathZZ = strdup(DirPath);
	DirPathZZ[strlen(DirPath) - 1] = '\0';
	
	SHStruct.hwnd = NULL;
	SHStruct.wFunc = FO_DELETE;
	SHStruct.pFrom = DirPathZZ;
	SHStruct.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;

	retval = SHFileOperation(&SHStruct);
	safe_free(DirPathZZ);

	if(retval != 0 || SHStruct.fAnyOperationsAborted){
		//wut?
		return FALSE;
	}
	
	//Delete directory itself now, I think. (If not, oh well. No harm.)
	RemoveDirectory(DirPath);

	return TRUE;
}
 
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
 *         Name:  File_Movtree
 *  Description:  Moves a directory and all files and folders within.
 * =====================================================================================
 */
 
BOOL File_MovTree(char *srcPath, char *dstPath)
{
	//Actually stupidly simple. Cross-platform, too.
	return rename(srcPath, dstPath);
}

//Create the given file with the given length
BOOL File_Create(char *FilePath, int FileLen)
{
	int handle = _creat(FilePath, S_IWRITE);
	const char *pattern = '\0';
	
	if(handle == -1){
		ErrNo2ErrCode();
		return FALSE;
	}
	
	File_WritePattern(handle, 0, pattern, sizeof(pattern), FileLen);
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_TransactStart
 *  Description:  Begins writing a file in a safe manner in the event
 *                an uninstall is required. Win32 does this by copying
 *                the given file to a BACKUP directory.
 * =====================================================================================
 */
BOOL File_TransactStart(const char *FileName, const char *ModUUID)
{
	char *src = NULL, *dst = NULL, *newUUID;
	newUUID = Mod_MangleUUID(ModUUID);
	asprintf(&src, "%s/%s", CONFIG.CURRDIR, FileName);
	asprintf(&dst, "%s/BACKUP/%s/%s", CONFIG.CURRDIR, newUUID, FileName);
	
	//Succeed if FileName is :memory:
	if(strcmp(FileName, ":memory:") == 0){
		safe_free(newUUID);
		safe_free(src);
		safe_free(dst);
		return TRUE;
	}
	
	//Fail if src is missing
	if(!access(src, R_OK | W_OK)){
		safe_free(newUUID);
		safe_free(src);
		safe_free(dst);
		return FALSE;
	}
	
	//Don't copy if transaction is in progress
	if(!access(dst, F_OK)){
		File_Copy(src, dst);
	}
	
	safe_free(newUUID);
	safe_free(src);
	safe_free(dst);
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_TransactWrite
 *  Description:  Marks [Bytes] as to be inserted into [FilePath] when
 *                File_TransactCommit() is called.
 *                Implemented by simply writing on Win32.
 * =====================================================================================
 */
BOOL File_TransactWrite(
	const char *FileName,
	int offset,
	unsigned const char *data,
	int datalen
){
	char *FilePath = NULL;
	int handle;
	
	//Do nothing if FileName is :memory:
	if(strcmp(FileName, ":memory:") == 0){
		return TRUE;
	}
	
	//Open file handle
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
	handle = File_OpenSafe(FilePath, _O_BINARY|_O_RDWR);
	safe_free(FilePath);
	if(handle == -1){
		return FALSE;
	} 
	
	//Wrap File_WriteBytes
	File_WriteBytes(handle, offset, data, datalen);
	
	close(handle);
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_TransactRollback
 *  Description:  Recovers the backed-up version of the file stored by
 *                File_TransactStart. Implemented by copying backup on Win32.
 * =====================================================================================
 */
void File_TransactRollback(const char *FileName, const char *ModUUID)
{
	char *src = NULL, *dst = NULL, *newUUID;
	newUUID = Mod_MangleUUID(ModUUID);
	asprintf(&dst, "%s/%s", CONFIG.CURRDIR, FileName);
	asprintf(&src, "%s/BACKUP/%s/%s", CONFIG.CURRDIR, newUUID, FileName);
	
	File_Copy(src, dst);
	File_Delete(src);
	
	safe_free(newUUID);
	safe_free(src);
	safe_free(dst);
	return;
}
 
 /* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_TransactCommit
 *  Description:  Makes changes to the given file final and removes backup.
 *                I want these backups, so this is utterly useless to me.
 * =====================================================================================
 */
/*void File_TransactCommit(const char *FileName, const char *ModUUID)
{
	char *FilePath = NULL;
	asprintf(&FilePath, "%s/%s", CONFIG.CURRPATH, FileName);
	File_Delete(FilePath);
	safe_free(FilePath);
	return;
}*/

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_TransactRollbackAll
 *  Description:  Recovers backups of all files modified by a mod.
 *                Implemented by copying backup on Win32.
 * =====================================================================================
 */
void File_TransactRollbackAll_Recur(const char *searchPath, const char *basePath)
{
	int result = 1;
	HANDLE hSearch = NULL;
	WIN32_FIND_DATA hData = {0};
	hSearch = FindFirstFile(searchPath, &hData);
	
	if(hSearch == INVALID_HANDLE_VALUE){return;} 
	
	while(result != 0){
		if(hData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY == 0){
			//Is a directory
			char *newPath = NULL;
			char *newBase = NULL;
			asprintf(&newPath, "%s/%s/*", searchPath, hData.cFileName);
			asprintf(&newBase, "%s/%s", basePath, hData.cFileName);

			File_TransactRollbackAll_Recur(newPath, basePath);

			safe_free(newPath);
			safe_free(newBase);
		} else {
			//Restore file
			char *srcPath = NULL;
			char *dstPath = NULL;
			asprintf(&srcPath, "%s/%s", searchPath, hData.cFileName);
			asprintf(&dstPath, "%s/%s", basePath, hData.cFileName);

			MoveFile(srcPath, dstPath);

			safe_free(srcPath);
			safe_free(dstPath);
		}
		result = FindNextFile(hSearch, &hData);
	}
	FindClose(hSearch);
}
 
void File_TransactRollbackAll(const char *ModUUID)
{
	//Iterate through BACKUP folder (TODO: Subfolders?)
	char *backupPath = NULL;
	char *newUUID = Mod_MangleUUID(ModUUID);
	
	asprintf(&backupPath, "%s/BACKUP/%s/*", CONFIG.CURRDIR, newUUID);
	File_TransactRollbackAll_Recur(backupPath, CONFIG.CURRDIR);
	safe_free(backupPath);
	return;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_WriteBytes
 *  Description:  Given an offset and a byte array of data write it to the given file.
 * =====================================================================================
 */
void File_WriteBytes(
	int filehandle,
	int offset,
	unsigned const char *data,
	int datalen
){
	lseek(filehandle, offset - 1, SEEK_SET);
	write(filehandle, data, datalen);
	return;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_WritePattern
 *  Description:  Given an offset, length and byte pattern, it will write the pattern
 *                to the file until the length is filled.
 * =====================================================================================
 */
void File_WritePattern(
	int filehandle,
	int offset,
	unsigned const char *data,
	int datalen,
	int blocklen
){
	int blockRemain = blocklen;
	
	while(blockRemain > 0){
		File_WriteBytes(
			filehandle,
			offset,
			data,
			max(datalen, blockRemain)
		);
		blockRemain -= max(datalen, blockRemain);
	}
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
/*double MultiMax(double count, ...)
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
}*/

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
	if(input == INVALID_HANDLE_VALUE){
		int error = GetLastError();
		return -1;
	}
		
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
 *                with the mod loader.
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
 *                Possible CURRERROR values:
 *                    errNOERR: No conflicts (TRUE)
 *                    errUSR_ABORT: User does not want to install (FALSE)
 *                    errUSR_CONFIRM: User wishes to replace (TRUE)
 *                    errCRIT_DBASE; errCRIT_MALLOC: Critical errors (FALSE)
 *                    errWNG_MODCFG: Use of reserved mod name (FALSE)
 * =====================================================================================
 */ 
BOOL Mod_CheckConflict(json_t *root)
{
	int oldver;
	BOOL FirstInstall = FALSE;
	char *UUID = JSON_GetStr(root, "UUID");
	char *name = JSON_GetStr(root, "Name");
	CURRERROR = errNOERR;

	// Is it on the blacklist?
	// This is more complicated than I had hoped for. Oh well.
	{
		json_t *GameCfg, *GameVerList, *GameVer;
		unsigned int i;
		GameCfg = JSON_Load(CONFIG.GAMECONFIG);

		//Find game version
		GameVerList = json_object_get(GameCfg, "Whitelist");
		json_array_foreach(GameVerList, i, GameVer){
			char *CurrVer = JSON_GetStr(GameVer, "Name");
			json_t *ModList, *CurrMod;
			unsigned int j;

			if(strcmp(CurrVer, CONFIG.GAMEVER) != 0){
				//Now do the same 
				safe_free(CurrVer);
				continue;
			} 

			//Now iterate through blacklist
			ModList = json_object_get(GameVer, "Mods");
			json_array_foreach(ModList, j, CurrMod){
				const char *CurrItem = json_string_value(CurrMod);
				if(strcmp(CurrItem, UUID) == 0){
					//A hit!
					char *buf = NULL;
					asprintf (&buf, "%s is implicitly installed in base executable.", name);
					AlertMsg (buf, "Mod Already Installed");
					safe_free(buf);
					CURRERROR = errWNG_MODCFG;
					return FALSE;
				}
			}

			safe_free(CurrVer);
			break;
		}
		
		json_decref(GameCfg);
	}
	
	//Ban if using reserved name MODLOADER@invisibleup as well
	if(strcmp(UUID, "MODLOADER@invisibleup") == 0){
		AlertMsg(
			"Mod UUID \"MODLOADER@invisibleup\" is "
			"reserved for system use.\nPlease don't use "
			"that UUID or any other \"@invisibleup\" UUID.",
			"Reserved UUID"
		);
		CURRERROR = errWNG_MODCFG;
		return FALSE;
	}
	
	
	// Check for conflicts
	{
		sqlite3_stmt *command;
		const char *query = "SELECT Ver FROM Mods WHERE UUID = ?;";
		CURRERROR = errNOERR;
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC)
		) != 0){
			safe_free(UUID);
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		   
		oldver = SQL_GetNum(command);
		//Check that GetNum succeeded
		if(CURRERROR != errNOERR){
			return FALSE;
		}
		if(oldver == -1){
			FirstInstall = TRUE;
		}
		
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		safe_free(UUID);
	}

	if (oldver != 0){
		// There IS a version installed
		int MBVal = IDYES; // Message box response
		int ver; // Version required
		
		//Check version number
		ver = JSON_GetInt(root, "Ver");
		//Automatically install if not installed at all
		if(FirstInstall == TRUE){
			oldver = ver;
		} 
		
		if(ver > oldver){
			char *buf = NULL;
			asprintf (&buf, 
				"The mod you have selected is a newer version of\n"
				"'%s', which you have already installed.\n"
				"Do you want to upgrade from version %d to version %d?",
				name, oldver, ver);
			if(buf == NULL){
				safe_free(name);
				CURRERROR = errCRIT_MALLOC;
				return FALSE;
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
				safe_free(name);
				CURRERROR = errCRIT_MALLOC;
				return FALSE;
			}
			MBVal = MessageBox (0, buf, "Mod Downgrade", MB_ICONEXCLAMATION | MB_YESNO);
			safe_free(buf);
		}
		else if (ver == oldver && FirstInstall == FALSE){
			char *buf = NULL;
			asprintf (&buf, "%s is already installed.", name);
			if(buf == NULL){
				safe_free(name);
				CURRERROR = errCRIT_MALLOC; 
				return FALSE;
			}
			AlertMsg (buf, "Mod Already Installed");
			MBVal = IDNO;
			safe_free(buf);
		}
		
		safe_free(name);
		if (MBVal == IDNO){
			//Do not replace
			CURRERROR = errUSR_ABORT;
			return FALSE;
		} 
		else{
			//Do replace
			CURRERROR = errUSR_CONFIRM;
			return TRUE;
		} 
	}
	
	return TRUE; //No conflict
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
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC) |
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
				sqlite3_bind_text(command, 1, ParentUUID, -1, SQLITE_STATIC)
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
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC) |
				sqlite3_bind_text(command, 2, ParentUUID, -1, SQLITE_STATIC)
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
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC) |
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

	//Check if using memory psuedofile
	if(strcmp(FileName, ":memory:") == 0){
		return 0;
	}
	
	//Get the ID
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, FileName, -1, SQLITE_STATIC)
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
	int IDCount, ID, FileLen;
	char *FilePath = NULL;
	struct ModSpace ClearSpc = {0};
	
	CURRERROR = errNOERR;
	
	//Get size of file
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
	FileLen = filesize(FilePath);
	safe_free(FilePath);
	
	//If :memory:, create virtual file with 2GB max length
	//(2GB is the default max address space for 32-bit Windows programs)
	if(strcmp(FileName, ":memory:") == 0){
		FileLen = LONG_MAX;
	}
	
	//If the file doesn't exist, there's an obvious mod configuration error
	if(FileLen <= 0){
		CURRERROR = errWNG_MODCFG;
		return -1;
	}
	
	//Get highest ID assigned
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return -1;
	}
	   
	IDCount = SQL_GetNum(command);
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		return -1;
	}
	//Check that GetNum succeeded
	if(IDCount == -1 || CURRERROR != errNOERR){
		return -1;
	}
	
	//New ID is highest ID + 1 (unless file is :memory:)
	//Pitfall here: No file entry found and no file entries peroid
	//both return 0.
	if(strcmp(FileName, ":memory:") == 0){
		ID = 0;
	} else {
		ID = IDCount + 1;;
	}
	
	//Insert new ID into database
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_int(command, 1, ID) |
		sqlite3_bind_text(command, 2, FileName, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return -1;
	}
	command = NULL;
	
	//Add a big ol' CLEAR space spanning the whole file
	asprintf(&ClearSpc.ID, "Base.%s", FileName);
	ClearSpc.FileID = ID; //;)
	ClearSpc.Start = 0;
	ClearSpc.End = FileLen;
	ClearSpc.Valid = TRUE;
	
	//ModOp_Clear(&ClearSpc, "MODLOADER@invisibleup"); // Too high level
	Mod_MakeSpace(&ClearSpc, "MODLOADER@invisibleup", TRUE);
	
	safe_free(ClearSpc.ID);
	
	//Return new ID in case the caller needs it
	return ID;
}

//Totally different from Mod_FPO, I swear
//Returns that file that contains PatchUUID
char * File_FindPatchOwner(const char *PatchUUID)
{
	char *out = NULL;
	
	sqlite3_stmt *command;
	const char *query = "SELECT Path FROM Files "
	                    "JOIN Spaces ON Files.ID = Spaces.File "
	                    "WHERE Spaces.ID = ?";
	CURRERROR = errNOERR;
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
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

//Returns true if FilePath points to a Win32 PE.
BOOL File_IsPE(const char *FilePath){
	int handle = -1;
	BOOL result = FALSE;
	
	//Open file
	handle = File_OpenSafe(FilePath, R_OK);
	if(handle == -1){
		return FALSE;
	}
	
	//Check if DOS header is OK
	{
		char buf[3] = {0};
		read(handle, buf, 2);
		if(strcmp(buf, "MZ") != 0){
			goto File_IsPE_Return;
		}
	}
	
	//Seek to PE header (might be dangerous. Eh.)
	{
		uint32_t offset = 0;
		lseek(handle, 0x3C, SEEK_SET);
		read(handle, &offset, sizeof(offset));
		lseek(handle, offset, SEEK_SET);
	}
	
	//Check PE header
	{
		char buf[4] = {0};
		char check[4] = {'P', 'E', 0, 0};
		read(handle, buf, 4);
		if(memcmp(buf, check, 4) != 0){
			goto File_IsPE_Return;
		}
	}
	
	//We're probably fine at this point, barring somebody intentionally
	//trying to break this by feeding us MIPS binaries or something.
	result = TRUE;
	
File_IsPE_Return:
	close(handle);
	return result;
}

//Converts Win32 PE memory location to raw file offset
int File_PEToOff(const char *FilePath, uint32_t PELoc)
{
	int result = PELoc;
	int handle = -1;
	int i = 0;
	int temp = 0;
	
	uint32_t BaseAddr = 0;
	uint32_t PEHeaderOff = 0;
	
	uint16_t NoSections = 0;
	uint32_t NoDataDir = 0;
	
	uint32_t LastFileOff = 0;
	uint32_t LastPEOff = 0;
	
	//Check if PE or just some random file
	if(!File_IsPE(FilePath)){
		return result;
	}
	
	//Open file
	handle = File_OpenSafe(FilePath, R_OK);
	if(handle == -1){
		return result;
	}
	
	//Seek to first section header
	lseek(handle, 0x3C, SEEK_SET);
	read(handle, &PEHeaderOff, sizeof(PEHeaderOff));
	lseek(handle, PEHeaderOff, SEEK_SET);
	
	//Get base address (kind of tough)
	lseek(
		handle, 
		(4 + 2 + 2 + 4 + 4 + 4 + 2 + 2) + //COFF header 
		(2 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4),
		SEEK_CUR
	);
	temp = _tell(handle);
	read(handle, &BaseAddr, sizeof(BaseAddr));
	
	//Get section count
	lseek(handle, PEHeaderOff + 6, SEEK_SET);
	temp = _tell(handle);
	read(handle, &NoSections, sizeof(&NoSections));
	
	//Skip past data directories
	lseek(
		handle, PEHeaderOff +
		4 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + //COFF header 
		2 + 1 + 1 + (4 * 9) + (2 * 6) + (4 * 4) +
		2 + 2 + (4 * 5), //mNumberOfRvaAndSizes
		SEEK_SET
	);
	read(handle, &NoDataDir, sizeof(NoDataDir));
	temp = _tell(handle);
	lseek(handle, 8 * NoDataDir, SEEK_CUR);
	temp = _tell(handle);
	
	//Parse section headers
	for(i = 0; i < NoSections; i++){
		uint32_t PERel, PEAbs, FileOff;
		
		//Get PE loc of section start
		lseek(handle, 12, SEEK_CUR);
		read(handle, &PERel, sizeof(PERel));
		PEAbs = PERel + BaseAddr;
		
		//Get file offset of section start
		lseek(handle, 4, SEEK_CUR);
		read(handle, &FileOff, sizeof(FileOff));
		
		//Go to next section header
		lseek(handle, 16, SEEK_CUR);
		
		//Check if we went past the section
		if(PELoc < PEAbs){break;}
		
		//Set Last* variables
		LastFileOff = FileOff;
		LastPEOff = PEAbs;
	}
	
	//Compute location (we're done!)
	if(i != 0){ //File offset is not before sections
		result = (PELoc - LastPEOff) + LastFileOff;
	}
	
	close(handle);
	return result;
}


//Converts Win32 PE raw file offset to memory location
//Very similar to PEToOff
int File_OffToPE(const char *FilePath, uint32_t FileLoc)
{
	int result = FileLoc;
	int handle = -1;
	int i = 0;
	
	uint32_t BaseAddr = 0;
	uint32_t PEHeaderOff = 0;
	
	uint16_t NoSections = 0;
	uint32_t NoDataDir = 0;
	
	uint32_t LastFileOff = 0;
	uint32_t LastPEOff = 0;
	
	//Check if PE or just some random file
	if(!File_IsPE(FilePath)){
		return result;
	}
	
	//Open file
	handle = File_OpenSafe(FilePath, R_OK);
	if(handle == -1){
		return result;
	}
	
	//Seek to first section header
	lseek(handle, 0x3C, SEEK_SET);
	read(handle, &PEHeaderOff, sizeof(PEHeaderOff));
	lseek(handle, PEHeaderOff, SEEK_SET);
	
	//Get base address (kind of tough)
	lseek(
		handle, 
		(4 + 2 + 2 + 4 + 4 + 4 + 2 + 2) + //COFF header 
		(2 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4),
		SEEK_CUR
	);
	read(handle, &BaseAddr, sizeof(BaseAddr));
	
	//Get section count
	lseek(handle, PEHeaderOff + 6, SEEK_SET);
	read(handle, &NoSections, sizeof(&NoSections));
	
	//Skip past data directories
	lseek(
		handle, PEHeaderOff +
		4 + 4 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + //COFF header 
		2 + 1 + 1 + (4 * 8) + (2 * 6) + (4 * 4) +
		2 + 2 + (4 * 5), //mNumberOfRvaAndSizes
		SEEK_SET
	);
	read(handle, &NoDataDir, sizeof(NoDataDir));
	lseek(handle, 8 * NoDataDir, SEEK_CUR);
	
	//Parse section headers
	for(i = 0; i < NoSections; i++){
		uint32_t PERel, PEAbs, FileOff;
		
		//Get PE loc of section start
		lseek(handle, 12, SEEK_CUR);
		read(handle, &PERel, sizeof(PERel));
		PEAbs = PERel + BaseAddr;
		
		//Get file offset of section start
		lseek(handle, 4, SEEK_CUR);
		read(handle, &FileOff, sizeof(FileOff));
		
		//Go to next section header
		lseek(handle, 16, SEEK_CUR);
		
		//Check if we went past the section
		if(FileLoc < FileOff){break;}
		
		//Set Last* variables
		LastFileOff = FileOff;
		LastPEOff = PEAbs;
	}
	
	//Compute location (we're done!)
	result = (FileLoc - LastFileOff) + LastPEOff;
	
	close(handle);
	return result;
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
		sqlite3_bind_text(command, 1, VarUUID, -1, SQLITE_STATIC)
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

//Get length, in bytes, of the given variable
int Var_GetLen(const struct VarValue *var)
{
	switch(var->type){
	case IEEE64:
		return 8;
	break;
		
	case Int32:
	case uInt32:
	case IEEE32:
		return 4;
	break;
	
	case Int16:
	case uInt16:
		return 2;
	break;
	
	case Int8:
	case uInt8:
		return 1;
	break;
	
	default:
		return 0;
	}
}

//Compare variable to integer/float/raw input according to string Mode.
BOOL Var_Compare(
	const struct VarValue *var, 
	const char *mode, 
	const void *input,
	const int inlen
){
	int varlen, memcmpRes;
	enum CompType{EQ, NOTEQ, LT, GT, LTE, GTE} comptype;
	
	//Get length of variable datatype
	varlen = Var_GetLen(var);
	if(varlen == 0){return FALSE;}
	
	varlen = min(varlen, inlen);
	
	//Store compare operation as enum
	if(strcmp(mode, "EQUALS") == 0){
		comptype = EQ;
	} else if (strcmp(mode, "NOT EQUALS") == 0){
		comptype = NOTEQ;
	} else if (strcmp(mode, "LESS") == 0){
		comptype = LT;
	} else if (strcmp(mode, "GREATER") == 0){
		comptype = GT;
	} else if (strcmp(mode, "LESS EQUAL") == 0){
		comptype = LTE;
	} else if (strcmp(mode, "GREATER EQUAL") == 0){
		comptype = GTE;
	} else {
		return FALSE;
	}
	
	//If float, do standard comparison
	if(var->type == IEEE32){
		float CondVal = *(float*)input;
		switch(comptype){
		case EQ:
			return (var->IEEE32 == CondVal);
		case NOTEQ:
			return (var->IEEE32 != CondVal);
		case LT:
			return (var->IEEE32 < CondVal);
		case GT:
			return (var->IEEE32 > CondVal);
		case LTE:
			return (var->IEEE32 <= CondVal);
		case GTE:
			return (var->IEEE32 >= CondVal);
		default:
			return FALSE;
		}
	} else if (var->type == IEEE64){
		double CondVal = *(double*)input;
		switch(comptype){
		case EQ:
			return (var->IEEE64 == CondVal);
		case NOTEQ:
			return (var->IEEE64 != CondVal);
		case LT:
			return (var->IEEE64 < CondVal);
		case GT:
			return (var->IEEE64 > CondVal);
		case LTE:
			return (var->IEEE64 <= CondVal);
		case GTE:
			return (var->IEEE64 >= CondVal);
		default:
			return FALSE;
		}
	}
	
	//Do memcmp comparisons
	memcmpRes = memcmp(var->raw, input, varlen);
	
	switch(comptype){
		case EQ:
			return (memcmpRes == 0);
		case NOTEQ:
			return (memcmpRes != 0);
		case LT:
			return (memcmpRes < 0);
		case GT:
			return (memcmpRes > 0);
		case LTE:
			return (memcmpRes <= 0);
		case GTE:
			return (memcmpRes >= 0);
		default:
			return FALSE;
	}
}

//Create Public Variable Listbox entires in SQL database
void Var_CreatePubList(json_t *PubList, const char *UUID)
{
	json_t *row;
	unsigned int i;
	CURRERROR = errNOERR;
	
	json_array_foreach(PubList, i, row){
		char *label = JSON_GetStr(row, "Label");
		int num = JSON_GetInt(row, "Value");
		sqlite3_stmt *command;
		char *query = "INSERT INTO VarList (Var, Number, Label) "
		              "VALUES (?, ?, ?);";
		
		//Get SQL results and put into VarObj
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC) |
			sqlite3_bind_int(command, 2, num) |
			sqlite3_bind_text(command, 3, label, -1, SQLITE_STATIC)
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

//Remove a variable entry and it's listbox entry
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
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
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
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
	}
	
	return TRUE;
}

//Update a variable
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
		sqlite3_bind_text(command, 2, result.UUID, -1, SQLITE_STATIC)
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

//Create a variable entry
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
			sqlite3_bind_text(command, 1, result.UUID, -1, SQLITE_STATIC) |
			sqlite3_bind_text(command, 2, result.mod, -1, SQLITE_STATIC) |
			sqlite3_bind_text(command, 3, Var_GetType_Str(result.type), -1, SQLITE_STATIC) |
			sqlite3_bind_text(command, 4, result.publicType, -1, SQLITE_STATIC) |
			sqlite3_bind_text(command, 5, result.desc, -1, SQLITE_STATIC) |
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

//Create a variable entry from mod configuration JSON
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

//Return TRUE if the given patch UUID is installed, FALSE otherwise
BOOL Mod_SpaceExists(const char *PatchUUID)
{
	sqlite3_stmt *command;
	const char *query = "SELECT EXISTS(SELECT * FROM Spaces WHERE ID = ?)";
	int result;
	CURRERROR = errNOERR;
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	result = SQL_GetNum(command);
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return FALSE;
	}
	
	return (result > 0);
}

//Returns a ModSpace struct that matches the given space
struct ModSpace Mod_GetSpace(const char *PatchUUID)
{
	struct ModSpace result = {0};
	json_t *out, *row;
	sqlite3_stmt *command;
	const char *query = "SELECT * FROM Spaces WHERE ID = ? ORDER BY Ver DESC"; 
	
	CURRERROR = errNOERR;
	result.Valid = FALSE;
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return result;
	}
	
	out = SQL_GetJSON(command);
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return result;
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
	result.FileID = JSON_GetInt(row, "File");
	result.Start = JSON_GetInt(row, "Start");
	result.End = JSON_GetInt(row, "End");
	result.ID = JSON_GetStr(row, "ID");
	
	result.Bytes = NULL;
	result.Len = result.End - result.Start;
	result.Valid = TRUE;

	return result;
}

//Returns the type of the given space
char * Mod_GetSpaceType(const char *PatchUUID)
{
	sqlite3_stmt *command;
	const char *query = "SELECT Type FROM Spaces WHERE ID = ? ORDER BY Ver DESC"; 
	char *out = NULL;
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return strdup("");
	}
	
	out = SQL_GetStr(command);
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return strdup("");
	}
	
	return out;
}
/*
//Returns the ROWID of the given space
//Turns out renaming all of history is really really slow
int Mod_GetRowIDFromSpace(const char *PatchUUID)
{
	sqlite3_stmt *command;
	const char *query = "SELECT ROWID FROM Spaces WHERE ID = ?"; 
	int out;
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return -1;
	}
	
	out = SQL_GetNum(command);
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return -1;
	}
	
	return out;
}

//Returns the spaces corresponding to the given ROWID
char * Mod_GetSpaceFromRowID(const char *PatchUUID)
{
	sqlite3_stmt *command;
	const char *query = "SELECT Type FROM Spaces WHERE ID = ?"; 
	char *out = NULL;
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return strdup("");
	}
	
	out = SQL_GetStr(command);
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return strdup("");
	}
	
	return out;
}
*/

//Renames a space in the SQL
BOOL Mod_RenameSpace(const char *OldID, const char *NewID)
{
	//We're purposely NOT using SQL_HandleErrors in case the caller
	//doesn't check if the new ID is already used.
	sqlite3_stmt *command;
	const char *query1 = "UPDATE Spaces SET ID = REPLACE(ID, ?, ?)";
	const char *query2 = "UPDATE Spaces SET Start = REPLACE(Start, ?, ?)";
	const char *query3 = "UPDATE Spaces SET End = REPLACE(End, ?, ?)";
	int SQLResult;
	
	SQLResult = sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL);
	
	sqlite3_bind_text(command, 1, OldID, -1, SQLITE_STATIC);
	sqlite3_bind_text(command, 2, NewID, -1, SQLITE_STATIC);
	
	SQLResult = sqlite3_step(command);
	sqlite3_finalize(command);
	
	if(!(
		SQLResult == SQLITE_OK ||
		SQLResult == SQLITE_DONE ||
		SQLResult == SQLITE_ROW
	)){
		return FALSE;
	}
		
	//If a MERGE or SPLIT space reference the old space, change ref
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__,
		sqlite3_bind_text(command, 1, OldID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 2, NewID, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query3, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__,
		sqlite3_bind_text(command, 1, OldID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 2, NewID, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	return TRUE;
}

// Mod_MakeBranchName
// Returns an "~x" name variant for new branch, with x being lowest possible

// Note: Normally, if you try to get the ~x" variant of an "~x" variant, they
// will stack, leaving an ~x~0". After enough of these (only a couple
// dozen of them are needed), the program slows to an absolute crawl
// due to manipulating gigantic strings thousands of bytes in length.
// This tries to prevent this by checking if any branches have been
// made at all, and if so, increments ~x.

// These were formerly known as "old names". Those were really complex
// and computationally expensive, due to the fact they needed to be edited
// retroactively to every patch that used them. (Pretty much imagine the
// existing version system except attached to the name and in reverse.)
// Instead I added the version field. Still kind of slow, but much better.

char * Mod_MakeBranchName(const char *PatchUUID)
{
	int oldCount = 1;
	char *result = NULL;
	int PatchUUIDLen = strlen(PatchUUID);
	char *BaseOldPtr;
	
	//Check for branch chaining
	//Strip off everything after last ~ in PatchUUID
	BaseOldPtr = strrchr(PatchUUID, '~');
	if(BaseOldPtr){
		char *BaseName = NULL;
		char *BaseOld = NULL;

		BaseName = strdup(PatchUUID);
		BaseName[BaseOldPtr - PatchUUID] = '\0';

		//See if any branches have been made
		asprintf(&BaseOld, "%s~1", BaseName);
		
		//if(strcmp(BaseOld, PatchUUID) == 0){
		if(Mod_SpaceExists(BaseOld)){
			//Branch chain! That's bad.
			result = Mod_MakeBranchName(BaseName);
			safe_free(BaseName);
			safe_free(BaseOld);
			return result;
		}

		safe_free(BaseName);
		safe_free(BaseOld);
	}

	while(TRUE){
		//Actually do thing thing this function was intended to do
		asprintf(&result, "%s~%d", PatchUUID, oldCount);

		//Does patch name exist already?
		if(Mod_SpaceExists(result)){
			//Increment oldCount
			//(We never directly use it's value, so it's okay.)
			oldCount += 1;
			safe_free(result);
		} else {
			break;
		}
	}
	return result;
}

//Claims a space in the name of a mod by setting the UsedBy thing
BOOL Mod_ClaimSpace(const char *PatchUUID, const char *ModUUID)
{
	sqlite3_stmt *command;
	const char *query = "UPDATE Spaces SET UsedBy = ? WHERE ID = ? "
	                    "AND Ver = ?;";
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 2, PatchUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 3, Mod_GetVerCount(PatchUUID))
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	return TRUE;
}

// Unclaims the given space by claiming it in the name of nothing.
// It's lazy, but it works.
BOOL Mod_UnClaimSpace(const char *PatchUUID)
{
	return Mod_ClaimSpace(PatchUUID, "");
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_FindSpace
 *  Description:  Finds free space in the file for modification.
 *                The given chunk of space is the smallest that
 *                is within the bounds given by the input.
 * =====================================================================================
 */
struct ModSpace Mod_FindSpace(const struct ModSpace *input, BOOL IsClear)
{
	struct ModSpace result = {0};
	json_t *out, *row;
	char *IDStr = NULL;
	
	//Set defaults for result
	CURRERROR = errNOERR;
	result.Valid = FALSE;

	//Do the finding
	{
		sqlite3_stmt *command;
		//Advanced SQL (Thanks, Ver.)
		const char *query = 
			"SELECT Spaces.* FROM Spaces "
			"LEFT OUTER JOIN Spaces b "
			"	ON Spaces.ID = b.ID AND Spaces.Ver < b.Ver "
			"WHERE b.ID IS NULL AND "
			"Spaces.Type = ? AND "
			"Spaces.Len >= ? AND Spaces.Start <= ? AND Spaces.End >= ? "
			"AND Spaces.File = ? ORDER BY Spaces.Len;";

		/* const char *query = "SELECT * FROM Spaces WHERE "
		                    "Type = ? AND (UsedBy IS NULL OR "
							"UsedBy == '') AND "
		                    "Len >= ? AND Start <= ? AND End >= ? "
		                    "AND File = ? ORDER BY Len;"; */

		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1,
				IsClear ? "Clear":"Add", -1, SQLITE_STATIC) ||
			sqlite3_bind_int(command, 2, input->Len) ||
			sqlite3_bind_int(command, 3, input->Start) ||
			sqlite3_bind_int(command, 4, input->End) ||
			sqlite3_bind_int(command, 5, input->FileID)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return result;
		}
		
		out = SQL_GetJSON(command);
		
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			return result;
		}
	}
	
	//Make sure json output is valid
	if(!json_is_array(out)){
		CURRERROR = errCRIT_DBASE;
		return result;
	}
	
	row = json_array_get(out, 0);
	
	//Pull data
	result.FileID = JSON_GetInt(row, "File");
	result.Start = JSON_GetInt(row, "Start");
	result.End = JSON_GetInt(row, "End");
	result.ID = JSON_GetStr(row, "ID");
	
	result.Bytes = NULL;
	result.Len = result.End - result.Start;
	result.Valid = TRUE;

	json_decref(out);
	return result;
}

struct ModSpace Mod_FindParentSpace(const struct ModSpace *input)
{
	//Note: Branches are non-issue. Branches fork off; they don't
	//replace the original.
	sqlite3_stmt *command;
	/*const char *query = "SELECT * FROM Spaces WHERE File = ? AND "
	                    "Start <= ? AND END >= ? AND ID NOT GLOB ?";*/
	const char *query = 
		"SELECT Spaces.* FROM Spaces "
		"LEFT OUTER JOIN Spaces b "
		"ON Spaces.ID = b.ID AND Spaces.Ver < b.Ver "
		"WHERE b.ID IS NULL AND Spaces.File = ? AND "
		"Spaces.Start <= ? AND Spaces.End >= ?; AND "
		"(Spaces.UsedBy IS NOT NULL AND Spaces.UsedBy != '')";

/*	const char *query = 
		"SELECT * FROM Spaces WHERE (UsedBy IS NOT NULL "
		"AND UsedBy != '') AND File = ? AND Start <= ? AND End >= ?";*/
	json_t *out, *row;

	//char *NewID = NULL;

	char *FileName = NULL;
	char *FilePath = NULL;

	struct ModSpace result = {0};
	CURRERROR = errNOERR;
	//asprintf(&NewID, "%s*", input->ID);

	//Convert input.Start and input.End to PE if needed
	/*FileName = File_GetName(input->FileID);
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
	if(File_IsPE(FilePath)){
		start = File_OffToPE(FilePath, input->Start);
		end = File_OffToPE(FilePath, input->End);
	}*/

	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_int(command, 1, input->FileID) |
		sqlite3_bind_int(command, 2, input->Start) |
		sqlite3_bind_int(command, 3, input->End) //|
		//sqlite3_bind_text(command, 3, NewID, -1, SQLITE_STATIC)
	) != 0){
		//safe_free(NewID);
		CURRERROR = errCRIT_DBASE;
		return result;
	}
	//safe_free(NewID);
	
	out = SQL_GetJSON(command);
	
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return result;
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
	result.FileID = JSON_GetInt(row, "File");
	result.Start = JSON_GetInt(row, "Start");
	result.End = JSON_GetInt(row, "End");
	result.ID = JSON_GetStr(row, "ID");
	
	result.Bytes = NULL;
	result.Len = result.End - result.Start;
	result.Valid = TRUE;

	return result;
}

//Return the value of the greatest version for PatchUUID
int Mod_GetVerCount(const char *PatchUUID)
{
	sqlite3_stmt *command;
	const char *query = "SELECT COUNT(*) FROM Spaces WHERE ID = ?";
	int verCount;
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		//safe_free(oldName);
		return -1;
	}
	
	verCount = SQL_GetNum(command);
	sqlite3_finalize(command);
	if(verCount == -1 && CURRERROR == errNOERR){
		verCount = 0;
	}
	
	return verCount;
}

char * Mod_FindPatchOwner(const char *PatchUUID)
{
	char *out = NULL;
	
	sqlite3_stmt *command;
	const char *query = "SELECT ID FROM Spaces "
	                    "JOIN Mods ON Spaces.Mod = Mods.UUID "
	                    "WHERE Spaces.ID = ? ORDER BY Spaces.Ver DESC";
	CURRERROR = errNOERR;
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
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


BOOL Mod_SplitSpace(
	struct ModSpace *out,
	const char *HeadID,
	const char *TailID,
	const char *OldID,
	const char *ModUUID,
	int splitOff,
	BOOL retHead
){
	char *OldPatchType = NULL;
	//char *HeadIDNew = NULL;
	//char *TailIDNew = NULL;
	char *OldIDNew = NULL;
	struct ModSpace OldPatch = {0};
	BOOL retval = FALSE;
	
	sqlite3_stmt *command;
	const char *query = "INSERT INTO Spaces "
		"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len', 'UsedBy', 'Ver') VALUES "
		"(?, ?, ?, ?, ?, ?, ?, ?, ?);";

	CURRERROR = errNOERR;
	
	//Sanity assertions
	//Oh. What do we do when we're splitting different versions?
	if(strcmp(HeadID, TailID) == 0){
		CURRERROR = errCRIT_ARGMNT;
		return FALSE;
	}
		
	//Get patch data for OldID
	OldPatch = Mod_GetSpace(OldID);
	OldPatchType = Mod_GetSpaceType(OldID);

	//More sanity assertions
	if(OldPatch.Valid == FALSE){
		CURRERROR = errCRIT_ARGMNT;
		return FALSE;
	}
	
	//I've lost sense of logic. Help.
	/*if(strcmp(HeadID, OldID) == 0){
		TailIDNew = strdup(TailID);
		//asprintf(&HeadIDNew, "%s~head", HeadID);
		HeadIDNew = Mod_MakeBranchName(HeadID);
	} else if(strcmp(TailID, OldID) == 0){
		HeadIDNew = strdup(HeadID);
		//asprintf(&TailIDNew, "%s~tail", TailID);
		TailIDNew = Mod_MakeBranchName(TailID);
	} else {*/
		//HeadIDNew = strdup(HeadID);
		//TailIDNew = strdup(TailID);
	//}
	
	if(
		strcmp(HeadID, OldID) == 0 ||
		strcmp(TailID, OldID) == 0
	){
		//We should have a unique ID for this internal bookkeeping junk
		asprintf(&OldIDNew, "%s-%s", HeadID, TailID);
	} else {
		OldIDNew = strdup(OldID);
	}

	/*//Check if HeadIDNew/TailIDNew already exists
	if(Mod_SpaceExists(HeadIDNew)){
		//Reold the existing space to prevent conflicts
		char *HeadIDNewer = Mod_MakeOldName(HeadIDNew);
		Mod_RenameSpace(HeadIDNew, HeadIDNewer);
		safe_free(HeadIDNewer);
	}
	if(Mod_SpaceExists(TailIDNew)){
		char *TailIDNewer = Mod_MakeOldName(TailIDNew);
		Mod_RenameSpace(TailIDNew, TailIDNewer);
		safe_free(TailIDNewer);
	}

	//Rename existing OldID to ~old variant (hopefully this works?)
	{
		char *OlderID = NULL;
		OlderID = Mod_MakeOldName(OldID);
		if(!Mod_RenameSpace(OldID, OlderID)){
			CURRERROR = errCRIT_DBASE;
			goto Mod_SplitSpace_Return;
		}
		safe_free(OlderID);
	}*/
	
	//Claim the original space in our name
	Mod_ClaimSpace(OldID, "MODLOADER@invisbleup");
	
	//Prepare DB
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_SplitSpace_Return;
	}
	
	//Create HeadID
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, HeadID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 2, OldPatchType, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 3, OldPatch.FileID) |
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 5, OldPatch.Start) |
		sqlite3_bind_int(command, 6, OldPatch.Start + splitOff) |
		sqlite3_bind_int(command, 7, splitOff) |
//		sqlite3_bind_text(command, 8, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_null(command, 8) |
		sqlite3_bind_int(command, 9, Mod_GetVerCount(HeadID) + 1)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_reset(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_SplitSpace_Return_PostDB;
	}
	
	//Create TailID
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, TailID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 2, OldPatchType, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 3, OldPatch.FileID) |
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 5, OldPatch.Start + splitOff + 1) |
		sqlite3_bind_int(command, 6, OldPatch.End) |
		sqlite3_bind_int(command, 7, OldPatch.Len - splitOff) |
//		sqlite3_bind_text(command, 8, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_null(command, 8) |
		sqlite3_bind_int(command, 9, Mod_GetVerCount(TailID) + 1)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_reset(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_SplitSpace_Return_PostDB;
	}
	
	//Create PatchUUID
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, OldIDNew, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 2, "Split", -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 3, OldPatch.FileID) |
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 5, HeadID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 6, TailID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 7, OldPatch.Len) |
		sqlite3_bind_text(command, 8, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 9, Mod_GetVerCount(OldIDNew) + 1)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_reset(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_SplitSpace_Return_PostDB;
	}
	
	//Write back positional data
	if(out && retHead){
		out->Start = OldPatch.Start;
		out->End = OldPatch.Start + splitOff;

		if(out->ID != HeadID && out->ID != TailID){
			safe_free(out->ID);
		}
		out->ID = strdup(HeadID);

	} else if (out && !retHead) {
		out->Start = OldPatch.Start + splitOff + 1;
		out->End = OldPatch.End;

		if(out->ID != HeadID && out->ID != TailID){
			safe_free(out->ID);
		}
		out->ID = strdup(TailID);
	}

/*	//Convert PE offsets to file offsets, if needed
	if(out){
		char *FileName = NULL;
		char *FilePath = NULL;

		FileName = File_GetName(OldPatch.FileID);
		asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);

		out->Start = File_PEToOff(FilePath, out->Start);
		out->End = File_PEToOff(FilePath, out->End);

		safe_free(FileName);
		safe_free(FilePath);
	}*/
	
	retval = TRUE;
	
Mod_SplitSpace_Return_PostDB:
	if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		retval = FALSE;
	}
Mod_SplitSpace_Return:
	//safe_free(HeadIDNew);
	//safe_free(TailIDNew);
	safe_free(OldPatch.ID);
	safe_free(OldPatchType);
	
	return retval;
}

// Merge the given two spaces into a larger space
// Currently broken and, frankly, useless.
/*BOOL Mod_MergeSpace(const struct ModSpace *input, const char *HeadID, const char *TailID, const char *ModUUID)
{
	struct ModSpace HeadPatch = {0};
	struct ModSpace TailPatch = {0};
	char *HeadType = NULL;
	char *TailType = NULL;
	
	char *AutoID = NULL;
	char *NewID = NULL;
	char *NewIDOld = NULL;
	char *HeadIDNew = NULL;
	char *TailIDNew = NULL;
	
	sqlite3_stmt *command;
	const char *query = "INSERT INTO Spaces "
		"('ID', 'Ver', 'Type', 'File', 'Mod', 'Start', 'End', 'Len', 'UsedBy') VALUES "
		"(?, ?, ?, ?, ?, ?, ?, ?, ?);";
	
	BOOL retval = FALSE;
	
	//Get Head and Tail information
	HeadPatch = Mod_GetSpace(HeadID);
	HeadType = Mod_GetSpaceType(HeadID);
	TailPatch = Mod_GetSpace(TailID);
	TailType = Mod_GetSpaceType(TailID);
	
	//Bail if we need to
	if(strcmp(HeadType, TailType) == 0){
		//Types not equal. Unsupported action.
		goto Mod_MergeSpace_Return;
	}
	
	if(
		strcmp(HeadType, "Merge") == 0 ||
		strcmp(HeadType, "Split") == 0
	){
		//One of the spaces is MERGE or SPLIT. This is unsupported.
		goto Mod_MergeSpace_Return;
	}
	
	//Create a new ID if we need to
	asprintf(&AutoID, "%d-%X", input->FileID, input->Start);
	if(
		strcmp(input->ID, "") == 0 || //No ID
		strcmp(input->ID, AutoID) == 0 //Auto-generated ID

	){
		asprintf(&NewID, "%s-%s", HeadID, TailID);
	} else {
		NewID = strdup(input->ID);
	}
	safe_free(AutoID);

	//Mark Head and Tail as used.
	Mod_ClaimSpace(HeadID, ModUUID);
	Mod_ClaimSpace(TailID, ModUUID);
	
	//Bang out some SQL (Merge space, then [type] space)
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_MergeSpace_Return;
	}

	if(SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, NewID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 2, 0) | // Version
		sqlite3_bind_text(command, 3, "Merge", -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 4, HeadPatch.FileID) |
		sqlite3_bind_text(command, 5, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 6, HeadID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 7, TailID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 8, 0) |
		sqlite3_bind_text(command, 9, ModUUID, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_reset(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_MergeSpace_Return;
	}
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, NewID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 2, 1) | // Version
		sqlite3_bind_text(command, 3, HeadType, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 4, HeadPatch.FileID) |
		sqlite3_bind_text(command, 5, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 6, HeadPatch.Start) |
		sqlite3_bind_int(command, 7, TailPatch.End) |
		sqlite3_bind_int(command, 8, TailPatch.End - HeadPatch.Start) |
		sqlite3_bind_text(command,98, ModUUID, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_reset(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_MergeSpace_Return;
	}
	
	sqlite3_finalize(command);
	
	retval = TRUE;
	
	
Mod_MergeSpace_Return:
	safe_free(NewID);
	safe_free(HeadPatch.ID)
	safe_free(HeadType);
	safe_free(TailPatch.ID);
	safe_free(TailType);
	
	return retval;
}*/

//Splice one space into another space using SplitSpace.
BOOL Mod_SpliceSpace(
	struct ModSpace *parent,
	struct ModSpace *child,
	const char *ModUUID
){
/*	int IDRow;
	sqlite3_stmt *command;
	const char *query1 = "SELECT ROWID FROM Spaces WHERE ID = ? ORDER BY Ver DESC";
	const char *query2 = "UPDATE Spaces SET Type = 'Clear' WHERE ROWID = ?";

	//Save Row ID of that space (can't rely on standard ID w/ splits)
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, parent->ID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	IDRow = SQL_GetNum(command);
	sqlite3_finalize(command);*/
	char *parentName = strdup(parent->ID);
	char *childName = strdup(child->ID);
	char *PatchName = parentName;

	//Things break horribly if IDs point to the same thing
	if(parent->ID == child->ID){
		parent->ID = strdup(child->ID);
	}
	
	//Create head section
	if(child->Start != parent->Start){
		Mod_SplitSpace(
			parent, //ModSpace
			parentName, //Head
			childName, //Tail
			parentName,//Patch ID
			ModUUID,
			child->Start - parent->Start, //SplitOff
			FALSE //Return head?
		);

		// Splitting messes up the names, so we have to adjust.
		//PatchName = Mod_MakeBranchName(childName);
		PatchName = childName;
		parentName = Mod_MakeBranchName(parentName);
		//childName = Mod_MakeBranchName(childName);
		//Mod_RenameSpace(PatchName, childName);
		//Mod_RenameSpace(PatchName, childName);
	}
	
	//Create tail section
	if(child->End != parent->End){
		Mod_SplitSpace(
			parent, //ModSpace
			childName, //Head
			parentName, //Tail
			PatchName, //Patch ID
			//child->ID,
			ModUUID,
			child->End - parent->Start, //SplitOff
			TRUE //Return head?
		);
	}

	safe_free(parentName);
	safe_free(childName);
	PatchName = NULL;

	//Convert that ADD space we made to a CLEAR space
	//(The SplitSpaces above pulled from this space, so we needed
	// to update it afterwards. This space isn't necessary, but it
	// keeps the EXE counter working.)
	
/*	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_int(command, 1, IDRow)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}*/

	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_MakeSpace
 *  Description:  Creates an occupied space marker in the given space.
 *                No sanity checking is done whatsoever.
 * =====================================================================================
 */
BOOL Mod_MakeSpace(struct ModSpace *input, const char *ModUUID, BOOL Clear){
	//Compute the wanted Start address and End address
	//Tries to go as low as possible, but sometimes the ranges clash.
	struct ModSpace old = {0};
	sqlite3_stmt *command;
	const char *query = "INSERT INTO Spaces "
		"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len', 'Ver') VALUES "
		"(?, ?, ?, ?, ?, ?, ?, ?);";
	char *FilePath = NULL, *FileName = NULL;
	int PEStart, PEEnd, Ver;

	CURRERROR = errNOERR;

	//Convert input.Start and input.End to PE format if needed
	FileName = File_GetName(input->FileID);
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);

	//Convert to PE offset
	if(File_IsPE(FilePath)){
		PEStart = File_OffToPE(FilePath, input->Start);
		PEEnd = File_OffToPE(FilePath, input->End);
	} else {
		PEStart = input->Start;
		PEEnd = input->End;
	}

	safe_free(FileName);
	safe_free(FilePath);
	
	//Get value of VER
	Ver = Mod_GetVerCount(input->ID) + 1;
	if(Ver == -1){return FALSE;}

	//If replacing something, mark that space as used
	/*old = Mod_GetSpace(input->ID);
	if(old.Valid == TRUE){
		char *oldName = Mod_MakeOldName(old.ID);
		
		//Rename and claim space
		Mod_RenameSpace(old.ID, oldName);
		Mod_ClaimSpace(oldName, ModUUID);
		
		safe_free(oldName);
		
	}
	safe_free(old.ID);*/
	
	if(Ver > 0){
		Mod_ClaimSpace(input->ID, ModUUID);
	}
	
	//Create new AddSpc table row
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, input->ID, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 2, Clear ? "Clear" : "Add", -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 3, input->FileID) |
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 5, PEStart) |
		sqlite3_bind_int(command, 6, PEEnd) |
		sqlite3_bind_int(command, 7, PEEnd - PEStart) |
		sqlite3_bind_int(command, 8, Ver) 
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	command = NULL;
	return TRUE;
}

//Read existing bytes into revert table and write new bytes in
BOOL Mod_CreateRevertEntry(const struct ModSpace *input)
{
/*	unsigned char *OldBytesRaw = NULL;
	char *OldBytes = NULL;
	char *FileName = NULL, *FilePath = NULL;
	int handle = -1;
	sqlite3_stmt *command;
	const char *query = "INSERT INTO Revert "
		"('PatchUUID', 'Ver', 'OldBytes') VALUES (?, ?, ?);";
	BOOL retval = FALSE;
	
	//If using virtual memory file, exit.
	if(input->FileID == 0){
		retval = TRUE;
		goto Mod_CreateRevertEntry_Return;
	}
	
	//Read the old bytes
	OldBytesRaw = calloc(input->Len, sizeof(char));
	//Quick error checking
	if(OldBytesRaw == NULL){
		CURRERROR = errCRIT_MALLOC;
		goto Mod_CreateRevertEntry_Return;
	}
	
	//Get file path from input struct
	FileName = File_GetName(input->FileID);
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
	
	//Open handle
	handle = File_OpenSafe(FilePath, R_OK | W_OK);
	if(handle == -1){
		goto Mod_CreateRevertEntry_Return;
	}

	//Actually read the file. (I actually forgot this.)
	lseek(handle, input->Start - 1, SEEK_SET);
	read(handle, OldBytesRaw, input->Len);

	close(handle);
	handle = -1;
	
	//Convert to hex string
	OldBytes = Bytes2Hex(OldBytesRaw, input->Len);
	safe_free(OldBytesRaw);
	
	//Construct & Execute SQL Statement
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, input->ID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 2, Mod_GetVerCount(input->ID)) |
		sqlite3_bind_text(command, 3, OldBytes, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_CreateRevertEntry_Return;
	}
	command = NULL;
	retval = TRUE;
	
	
Mod_CreateRevertEntry_Return:
	safe_free(OldBytes);
	safe_free(FileName);
	safe_free(FilePath);
	
	return retval;
*/
	return TRUE;
}

//Best way to do this is to go ahead and produce a mod with Mov operations
//that can be uninstalled like any other mod.
//(MOVE won't work. But we'd probably want to mark something when we merge.)
//(Add MERGE/SPLIT spaces to SQL? Might be needed.)

///NEW
//Search for where Space.End = DifferentSpace.Start
	//Delete DifferentSpace
//Loop through results
	//Set Space.End to DifferentSpace.End
	//Recalculate Len
//Free n' shizz

///OLD
/*For every file id:
	struct ModSpace[] resultArray =
	SELECT * FROM ClearSpc WHERE
	'FileID' = [fileid]
	ORDER BY 'Start';
	
	Look at next ClearSpcID.
	See if first END is more than next START.
	If so, combine the two*/
	
// Too computationally expensive and likely to screw up.
// I'll deal with this later.

/*BOOL Mod_MergeFreeSpace()
{
	// Just for the record, this query is extremely slow.
	// (Takes at least 2.5 seconds to run for me.)
	// Use sparingly.
	const char *query = 
		"SELECT a.ID AS A, b.ID AS B FROM Spaces a "
		"JOIN Spaces b "
		"WHERE a.Type = 'Clear' AND b.Type = 'Clear' AND a.ID != b.ID "
		"AND (a.Start = b.End OR a.Start = b.End+1) "
		"AND a.Ver = (SELECT MAX(Ver) FROM Spaces WHERE Spaces.ID = a.ID) "
		"AND b.Ver = (SELECT MAX(Ver) FROM Spaces WHERE Spaces.ID = b.ID) ";
	sqlite3_stmt *command;
	json_t *out, *row;
	size_t i;
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 ){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	out = SQL_GetJSON(command);
	sqlite3_finalize(command);
	
	json_array_foreach(out, i, row){
		//Merge A and B (no-repeat guarantee)
		Mod_MergeSpace(
	}
	
	return TRUE;
	
}*/

// Delete any files are are filled with only Clear spaces
// Because who needs those?
BOOL Mod_RemoveAllClear(){
	// Query could use EXISTS, but I can't figure out how to do that.
	// Probably should, because this takes about 0.4s to run.
	const char *query = 
		"SELECT a.File, COUNT(*) AS Count FROM Spaces a "
		"LEFT OUTER JOIN Spaces b ON a.ID = b.ID AND a.Ver < b.Ver "
		"WHERE b.ID IS NULL AND a.File != 0 AND a.Type != 'Clear' "
		"AND (a.UsedBy = '' OR a.UsedBy IS NULL) "
		"GROUP BY a.File ";
	sqlite3_stmt *command;
	json_t *out, *row;
	size_t i;
	
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 ){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	out = SQL_GetJSON(command);
	sqlite3_finalize(command);
	
	json_array_foreach(out, i, row){
		//Get file path
		char *FileName = NULL;
		char *FilePath = NULL;
		char *BakPath = NULL;
		int FileID;
		
		FileID = JSON_GetInt(row, "File");
		FileName = File_GetName(FileID);
		asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
		asprintf(&BakPath, "%s/BACKUP/%s", CONFIG.CURRDIR, FileName);
		
		// Move the file to a backup directory
		File_MovTree(FilePath, BakPath);
		
		//(This will get restored when the restorer writes to it)
	}
	return TRUE;	
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ModOp_Reserve
 *  Description:  Finds and creates an Add space for the given input.
 * =====================================================================================
 */
BOOL ModOp_Reserve(struct ModSpace *input, const char *ModUUID){
	struct ModSpace FreeSpace;
	sqlite3_stmt *command;
	const char *query = "UPDATE Spaces SET Type = 'Add' WHERE ID = ? AND Ver = ?";

	//Find some free space to put it in
	FreeSpace = Mod_FindSpace(input, TRUE);
	
	if(FreeSpace.Valid){
		//Alter input to match found space
		input->Start = max(input->Start, FreeSpace.Start);
		input->End = min(input->Start + input->Len, FreeSpace.End);
	}
	
	if(
		!FreeSpace.Valid //||  //If no free space
		//input->End - input->Start != input->Len //Free space too small for splice
	){
		if(CURRERROR == errNOERR){
			AlertMsg(
				"There is not enough space cleared in the file to install\n"
				"this mod. Try removing some mods or installing some\n"
				"space-clearing mods to help make room.\n"
				"The installation cannot continue.",
				"Mod Installation Error"
			);
		}
		safe_free(FreeSpace.ID);
		return FALSE;
	}
	
	//Splice input into FreeSpace
	Mod_SpliceSpace(&FreeSpace, input, ModUUID);

	if(CURRERROR != errNOERR){
		safe_free(FreeSpace.ID);
		return FALSE;
	}
	
	//Turn our child into an add space
//	if(!Mod_MakeSpace(input, ModUUID, FALSE)){
//		return FALSE;
//	}

	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, input->ID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 2, Mod_GetVerCount(input->ID))
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		safe_free(FreeSpace.ID);
		return FALSE;
	}

	// Add a Start variable.
	{
		struct VarValue startvar = {0};
		startvar.type = uInt32;
		startvar.mod = strdup(ModUUID);
		startvar.uInt32 = input->Start;
		startvar.persist = FALSE;
		asprintf(&(startvar.UUID), "Start.%s.%s", input->ID, ModUUID);

		Var_MakeEntry(startvar);
		safe_free(startvar.UUID);
		safe_free(startvar.mod);
	}
	
	safe_free(FreeSpace.ID);
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ModOp_Clear
 *  Description:  Creates an clear space marker given a ModSpace struct containing a 
 *                start address, end address and file. Very dumb in operation; make sure
 *                space is indeed ready to be cleared first.
 * =====================================================================================
 */
BOOL ModOp_Clear(struct ModSpace *input, const char *ModUUID){
	unsigned char *pattern = NULL;
	int patternlen = 0;
	
	char *FileName = NULL;
	char *FilePath = NULL;
	
	struct ModSpace parentSpace = {0};
	//char *parentType = NULL;
	
	BOOL retval = FALSE;
	CURRERROR = errNOERR;

	FileName = File_GetName(input->FileID);
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
	
	//Find space that this space would be contained within
	parentSpace = Mod_FindParentSpace(input);
	
	//Convert parentSpace from PE offsets if needed
	parentSpace.Start = File_PEToOff(FilePath, parentSpace.Start);
	parentSpace.End = File_PEToOff(FilePath, parentSpace.End);

	//Are we actually occupying multiple spaces?
	if(parentSpace.Valid == FALSE) {
		sqlite3_stmt *command;
		const char *query = 
			"SELECT a.ID AS ID, a.Start AS Start, a.End AS End FROM Spaces a "
			"LEFT OUTER JOIN Spaces b ON a.ID = b.ID AND a.Ver < b.Ver "
			"WHERE b.ID IS NULL AND "
			"(a.End >= ? AND a.Start <= ?) AND "
			"a.File = ? AND (a.UsedBy = '' OR a.UsedBy IS NULL) "
			"ORDER BY a.Start ASC";
		json_t *out, *row;
		size_t i;
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__,
			sqlite3_bind_int(command, 1, input->Start) |
			sqlite3_bind_int(command, 2, input->End) |
			sqlite3_bind_int(command, 3, input->FileID)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			goto ModOp_Clear_Return;
			return FALSE;
		}
		
		out = SQL_GetJSON(command);
		if(json_array_size(out) == 0){
			//There's no spaces. Like, at all. No good.
			CURRERROR = errWNG_MODCFG;
			goto ModOp_Clear_Return;
			return FALSE;
		}
		sqlite3_finalize(command);
		
		//Make our own parent space
		memcpy(&parentSpace, input, sizeof(struct ModSpace));
		
		// What the lowest one? That's Start.
		row = json_array_get(out, 0);
		parentSpace.Start = JSON_GetInt(row, "Start");
		
		// What's the highest one? That's End.
		row = json_array_get(out, json_array_size(out) - 1);
		parentSpace.End = JSON_GetInt(row, "End");
		
		// Mark all of the overlapping spaces as used and invalid.
		json_array_foreach(out, i, row){
			char *ID = JSON_GetStr(row, "ID");
			Mod_ClaimSpace(ID, ModUUID);
			safe_free(ID);
		}
		// (Who needs MERGE, anyways?)
		
		// Make a new add space overlapping all of them
		if(!Mod_MakeSpace(&parentSpace, ModUUID, FALSE)){
			goto ModOp_Clear_Return;
		}

		json_decref(out);
		
		// Now continue as if we had a parent space this entire time.
	}
	
	{
		sqlite3_stmt *command;
		const char *query = "UPDATE Spaces SET Type = 'Clear' WHERE ID = ? AND Ver = ?";

		Mod_SpliceSpace(&parentSpace, input, ModUUID);
		if(CURRERROR != errNOERR){goto ModOp_Clear_Return;}

		// I guess SpliceSpace screws this up
		parentSpace.Start = File_PEToOff(FilePath, parentSpace.Start);
		parentSpace.End = File_PEToOff(FilePath, parentSpace.End);

		input->Start = parentSpace.Start;
		input->End = parentSpace.End;
		input->Len = input->End - input->Start;
		safe_free(input->ID);
		input->ID = parentSpace.ID;
		
		//Mark space as clear
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__,
			sqlite3_bind_text(command, 1, input->ID, -1, SQLITE_STATIC) |
			sqlite3_bind_int(command, 2, Mod_GetVerCount(input->ID))
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			goto ModOp_Clear_Return;
			return FALSE;
		}
	}
	
	//Create revert table entry
	Mod_CreateRevertEntry(input);
	
	//Write pattern depending on file contents
	if(File_IsPE(FilePath)){
		//Use UD2 (x86 undefined operation)
		unsigned short UD2 = 0x0F0B;
		patternlen = 2;
		pattern = malloc(2);
		memcpy(pattern, &UD2, 2);
	} else {
		//Use generic 0xFF
		unsigned char EMPTY = 0xFF;
		patternlen = 1;
		pattern = malloc(1);
		memcpy(pattern, &EMPTY, 1);
	}
	
	if(input->FileID != 0){
		int handle = File_OpenSafe(FilePath, _O_BINARY|_O_RDWR);
		File_WritePattern(handle, input->Start, pattern, patternlen, input->Len);
		close(handle);
	}
	
	retval = TRUE;
	
ModOp_Clear_Return:
	safe_free(FilePath);
	safe_free(FileName);
	//safe_free(parentSpace.ID);
	//safe_free(parentType);

	return retval;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ModOp_Add
 *  Description:  Add operation for mod installation.
 *                Applies to "Add", "Repl" and "Copy" operations.
 *                Finds free space in given file and writes given bytes to it.
 * =====================================================================================
 */
BOOL ModOp_Add(struct ModSpace *input, const char *ModUUID){

	//"Reserve" input space. (ADD is just RESERVE with bytes attached)
	if(!ModOp_Reserve(input, ModUUID)){
		return FALSE;	
	};
	
	//Create revert table entry
	Mod_CreateRevertEntry(input);
	
	if(input->FileID != 0){
		//Only write if not using memory pseudofile
		int handle = -1;
		char *FileName = NULL;
		char *FilePath = NULL;

		FileName = File_GetName(input->FileID);
		asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);

		handle = File_OpenSafe(FilePath, _O_BINARY|_O_RDWR);
		File_WriteBytes(handle, input->Start, input->Bytes, input->Len);
		close(handle);

		safe_free(FileName);
		safe_free(FilePath);
	}
	return TRUE;
}

//On failure, output guaranteed to be returned without calls to free() being required
//Fills a ModSpace with all the information needed to execute a patch
//
//General flow is this:
//	- Fill FileID
//	- Fill Start/End either from number or UUID
//	- If no ID given or mode is REPL:
//		- If Start is numeric, set ID to hex representation of Start
//		- If Start is string, set ID to Start
//	- If mode is COPY or MOVE:
//		- Fill SrcStart, SrcEnd, SrcFile
//		- Set Len = SrcEnd - SrcStart
//	- If mode is ADD or REPL:
//		- Fill Bytes with given value
//		- Set Len to length of Bytes
struct ModSpace Mod_GetPatchInfo(json_t *patchCurr, const char *ModPath)
{
	struct ModSpace input = {0}; //Return value
	char *Mode = NULL; //Patch mode
	char *FileName = NULL, *FilePath = NULL, *FileType = NULL;
	int debugStart, debugEnd;
	CURRERROR = errNOERR;
	input.Valid = FALSE;
	
	//Get Mode
	Mode = JSON_GetStr(patchCurr, "Mode");
	if(strcmp(Mode, "") == 0){
		//Mode was not found
		AlertMsg("`Mode` not defined for patch.", "JSON Error");
		goto Mod_GetPatchInfo_Failure;
	}
	
	///Set FileID and FileType
	//(By this point FileName is guaranteed to be good)
	FileName = JSON_GetStr(patchCurr, "File");
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
	input.FileID = File_GetID(FileName);
	FileType = JSON_GetStr(patchCurr, "FileType");

	//Init ID to NULL for later use
	input.ID = NULL;
	 
	///Set Start, End
	//Make sure Start exists
	/*if(!Mod_PatchKeyExists(patchCurr, "Start", TRUE)){
		CURRERROR = errWNG_MODCFG;
		goto Mod_GetPatchInfo_Failure;
	}*/
	
	//Get value of Start and End
	input.Start = JSON_GetInt(patchCurr, "Start");
	input.End = JSON_GetInt(patchCurr, "End");

	debugStart = input.Start;
	debugEnd = input.End;
	
	if(	//Start is UUID
		!json_is_integer(json_object_get(patchCurr, "Start")) && //Is text
		Mod_PatchKeyExists(patchCurr, "SrcStart", FALSE) &&
		input.Start == 0 //Is not text representing a number
	){
		char *UUID = JSON_GetStr(patchCurr, "Start");
		char *PatchID = JSON_GetStr(patchCurr, "ID");
		//Replace Start and End with UUID's start/end
		Mod_PatchFillUUID(
			&input, FALSE, FileType, FileName, FilePath, UUID
		);
		
		//If missing, set ID
		if(!PatchID || strcmp(PatchID, "") == 0){
			input.ID = UUID;
			safe_free(PatchID);
		} else {
			input.ID = PatchID;
			safe_free(UUID);
		}
		
	} else if (	//Start is number
		input.Start != 0 //Is not text representing a number
	){
		char *temp = NULL, *PatchID = NULL;
		
		//Check that End and Len are defined
		if(
			!Mod_PatchKeyExists(patchCurr, "End", TRUE)
		){
			CURRERROR = errWNG_MODCFG;
			goto Mod_GetPatchInfo_Failure;
		}
		
		//Set value
//		input.Start = JSON_GetInt(patchCurr, "Start");
//		input.End = JSON_GetInt(patchCurr, "End") + 1;
		
		//Convert if FileType is set to "PE"
		if(strcmp(FileType, "PE") == 0 && input.FileID != 0){
			//Convert memory address to file offset
			input.Start = File_PEToOff(FilePath, input.Start);
			input.End = File_PEToOff(FilePath, input.End);
		}
		
		//Set ID if unset
		PatchID = JSON_GetStr(patchCurr, "UUID");
		if(!PatchID || strcmp(PatchID, "") == 0){
			safe_free(PatchID);
			asprintf(&input.ID, "%d-%X", input.FileID, input.Start);
		} else {
			input.ID = PatchID;
		}
		
		//Finish
		safe_free(FileType);
	} else {	// Start is NULL (whole-file op)
		char *PatchID;

		input.Start = 0;
		input.End = filesize(FilePath);

		AlertMsg("WHOLE-FILE OP.", "DEBUG");

		//Set ID if unset
		PatchID = JSON_GetStr(patchCurr, "UUID");
		if(!PatchID || strcmp(PatchID, "") == 0){
			safe_free(PatchID);
			asprintf(&input.ID, "%d-ALL", input.FileID);
		} else {
			input.ID = PatchID;
		}
		
	}
	
	//Make a new file if the file doesn't exist.
	if(!File_Exists(FilePath, FALSE, FALSE)){
		File_Create(FilePath, input.End);
	}

	//Check if Start and End are OOB.
	if(
		input.FileID != 0 && (input.Start > input.End ||
		filesize(FilePath) < input.End || 0 > input.Start)
	){
		//Fail
		AlertMsg("'Start/End' are out of file range.", "JSON Error");
		CURRERROR = errWNG_MODCFG;
		goto Mod_GetPatchInfo_Failure;
	}
	
	///Set Bytes, Len, SrcStart, SrcEnd (if applicable)
	//Copy/Move
	if(strcmp(Mode, "Move") == 0 || strcmp(Mode, "Copy") == 0){
		char *SrcFile = NULL, *SrcPath = NULL;
		char *SrcLoc = NULL, *SrcType = NULL;
		
		//Check that SrcStart exists
		/*if(!Mod_PatchKeyExists(patchCurr, "SrcStart", TRUE)){
			CURRERROR = errWNG_MODCFG;
			goto Mod_GetPatchInfo_Failure;
		}*/
	
		//Get source file
		SrcFile = JSON_GetStr(patchCurr, "SrcFile");
		SrcLoc = JSON_GetStr(patchCurr, "SrcFileLoc");
		SrcType = JSON_GetStr(patchCurr, "SrcFileType");
		if(!SrcFile || strcmp(SrcFile, "") == 0){
			// Source file is current file
			safe_free(SrcFile);
			SrcFile = strdup(FileName);
		}
		
		if(strcmp(SrcLoc, "Mod") == 0){
			//Mod archive
			asprintf(&SrcPath, "%s%s", ModPath, SrcFile);
		} else {
			//Game installation directory
			asprintf(&SrcPath, "%s/%s", CONFIG.CURRDIR, SrcFile);
		}
		if(strcmp(SrcPath, "") == 0){
			AlertMsg("Hold it!", "");
		}
		safe_free(SrcLoc);
		
		//Set SrcStart/SrcEnd
		if (
			!json_is_integer(json_object_get(patchCurr, "SrcStart")) &&
			Mod_PatchKeyExists(patchCurr, "SrcStart", FALSE) &&
			JSON_GetInt(patchCurr, "SrcStart") == 0
		){ //UUID
			char *UUID = JSON_GetStr(patchCurr, "SrcStart");
			Mod_PatchFillUUID(
				&input, TRUE, SrcType, SrcFile, SrcPath, UUID
			);
			safe_free(UUID);
		} else if (
			JSON_GetInt(patchCurr, "SrcStart") != 0
		){ //Start is a number
		
			//Make sure that SrcEnd exists
			if(!Mod_PatchKeyExists(patchCurr, "SrcEnd", TRUE)){
				CURRERROR = errWNG_MODCFG;
				goto Mod_GetPatchInfo_Failure;
			}
		
			//Get value
			input.SrcStart = JSON_GetInt(patchCurr, "SrcStart");
			input.SrcEnd = JSON_GetInt(patchCurr, "SrcEnd") + 1;
			
			//Check if FileType is set to "PE"
			if(
				strcmp(FileType, "PE") == 0 &&
				strcmp(FileName, ":memory:") != 0
			){
				//Convert memory address to file offset
				input.SrcStart = File_PEToOff(SrcLoc, input.SrcStart);
				input.SrcEnd = File_PEToOff(SrcLoc, input.SrcEnd);
			}
		} else {
			//SrcStart / SrcEnd are whole-file
			input.SrcStart = 0;
			input.SrcEnd = filesize(SrcPath);
		}
		input.Len = input.SrcEnd - input.SrcStart;
		
		//Check if SrcStart and SrcEnd are OOB.
/*		if(
			strcmp(SrcFile, ":memory:") != 0 &&
			(input.SrcStart < input.SrcEnd || 0 >= input.SrcStart ||
			filesize(SrcPath) <= input.End)
		){
			//Fail
			AlertMsg("'SrcStart/SrcEnd' are out of file range.", "JSON Error");
			CURRERROR = errWNG_MODCFG;
			goto Mod_GetPatchInfo_Failure;
		}*/
		
		//Allocate space for bytes
		input.Bytes = calloc(input.Len, sizeof(char));
		if(input.Bytes == NULL){
			CURRERROR = errCRIT_MALLOC;
			goto Mod_GetPatchInfo_Failure;
		}
		
		//Set value of Bytes to value of bytes from SrcStart to SrcEnd
		{
			int fhandle = File_OpenSafe(SrcPath, _O_BINARY | _O_RDONLY);
			if(fhandle == -1){
				safe_free(input.Bytes);
				CURRERROR = errCRIT_FUNCT;
				goto Mod_GetPatchInfo_Failure;
			}
			
			lseek(fhandle, input.SrcStart, SEEK_SET);
			if (read(fhandle, input.Bytes, input.Len) != input.Len){
				safe_free(input.Bytes);
				CURRERROR = errCRIT_FUNCT;
				goto Mod_GetPatchInfo_Failure;
			}
			close(fhandle);
		}

		safe_free(SrcPath);
		safe_free(SrcFile);
		safe_free(SrcType);
	}
	
	//Add, Repl
	if(strcmp(Mode, "Add") == 0 || strcmp(Mode, "Repl") == 0){
		char *ByteStr = NULL, *ByteMode = NULL;
		
		//Check if ByteMode and ByteStr exist
		if(
			!Mod_PatchKeyExists(patchCurr, "AddType", TRUE) ||
			!Mod_PatchKeyExists(patchCurr, "Value", TRUE)
		){
			CURRERROR = errWNG_MODCFG;
			goto Mod_GetPatchInfo_Failure;
		}
		
		//Get Byte Mode and String.
		ByteStr = JSON_GetStr(patchCurr, "Value");
		ByteMode = JSON_GetStr(patchCurr, "AddType");
		
		///Possible byte modes:
		//  "Bytes"       - Raw data in hex form to be copied exactly.
		//                  Use for very short snippets of data or code.
		//  "UUIDPointer" - Give it the UUID of a space and it will give the
		//                  allocated memory address. For subroutines, etc.
		//  "VarValue"    - Give it the UUID of a mod loader variable 
		//                  and it will give it's contents.
		if (strcmp(ByteMode, "Bytes") == 0){
			//Convert the hex value of Value to bytes
			input.Bytes = Hex2Bytes(ByteStr, &input.Len);
			
		} else if (strcmp(ByteMode, "UUIDPointer") == 0){
			int start, end, add = 0;
			char *ptrplus, *ptrminus;
			//Find address/length of ID
			if(!Mod_FindUUIDLoc(&start, &end, ByteStr)){
				//Given UUID not found
				char *msg = NULL;
				asprintf(
					&msg, "Patch &s not installed.",
					ByteStr
				);
				AlertMsg(msg, "JSON Error");
				safe_free(msg);
				CURRERROR = errWNG_MODCFG;
				goto Mod_GetPatchInfo_Failure;
			}
			
			
			//Patch size MUST be 4 bytes.
			input.Len = 4; //Size of 32-bit ptr.
			
			//Allocate space for bytes
			input.Bytes = calloc(4, sizeof(char));
			if(input.Bytes == NULL){
				CURRERROR = errCRIT_MALLOC;
				goto Mod_GetPatchInfo_Failure;
			}
			
			//Add or subtract constant if needed
			ptrplus = strrchr(ByteStr, '+'); //Add
			ptrminus = strrchr(ByteStr, '-'); //Subtract
			if(ptrplus != NULL){
				add = atoi(ptrplus);
			} else if (ptrminus != NULL){
				add = atoi(ptrminus);
			}
			input.Bytes += add;
			
			//Set input.Bytes to value of start
			memcpy(input.Bytes, &start, 4);
			
		} else if (strcmp(ByteMode, "VarValue") == 0){
			//void *data = NULL;
			//int datalen = 0;
			struct VarValue var = {0};
			
			var = Var_GetValue_SQL(ByteStr);
			if(var.type == INVALID){
				char *msg = NULL;
				asprintf(
					&msg, "Variable &s not defined.",
					ByteStr
				);
				AlertMsg(msg, "JSON Error");
				safe_free(msg);
				CURRERROR = errWNG_MODCFG;
				goto Mod_GetPatchInfo_Failure;
			}
			
			input.Len = Var_GetLen(&var);
			memcpy(&input.Bytes, &var.raw, input.Len);
			
			safe_free(var.UUID);
			safe_free(var.desc);
			safe_free(var.publicType);
			safe_free(var.mod);
		}
		
		//Return without failure (Yes, this was necessary.)
		//input.Valid = TRUE;
		//goto Mod_GetPatchInfo_Return;
	}

	input.Valid = TRUE; //We reached end naturally, success
	goto Mod_GetPatchInfo_Return;
	
Mod_GetPatchInfo_Failure:
	safe_free(input.ID);
	safe_free(input.Bytes);
	
Mod_GetPatchInfo_Return:
	safe_free(Mode);
	safe_free(FileName);
	safe_free(FilePath);
	return input;
}

//Convienece wrapper to test if a value exists in a patch.
BOOL Mod_PatchKeyExists(json_t *patchCurr, const char *KeyName, BOOL ShowAlert)
{
	char *temp;
	BOOL retval = TRUE;
		
	//Make sure KeyName exists
	temp = JSON_GetStr(patchCurr, KeyName);
	if(temp == NULL || strcmp(temp, "") == 0){
		if(ShowAlert){
			char *msg = NULL;
			asprintf(&msg, "'%s' not defined for patch.", KeyName);
			AlertMsg(msg, "JSON Error");
			safe_free(msg);
		}
		retval = FALSE;
	}
	safe_free(temp);
	
	return retval;
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
		sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC)
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

//Fills the 'Start' and 'End' members of 'input' with the location of the
//given UUID. Errors if the UUID is from a different file than input.FileID.
//If 'IsSrc' is true, fills 'SrcStart' and 'SrcEnd' instead, checking 'SrcFile'
BOOL Mod_PatchFillUUID(
	struct ModSpace *input, BOOL IsSrc,
	const char *FileType, const char *FileName,
	const char *FilePath, const char *UUID
){
	char *UUIDFile = NULL;
	BOOL resultcode;
	int tempStart, tempEnd;
	
	//Get owner of UUID
	UUIDFile = File_FindPatchOwner(UUID);
	
	//Check if files are the same
	if(strcmp(UUIDFile, FileName) != 0){
		AlertMsg(
			"UUID in 'Start' is from different file than 'File'",
			"JSON Error"
		);
		safe_free(UUIDFile);
		return FALSE; //Different file. That's no good.
	}
	safe_free(UUIDFile);
	
	//Replace Start and End with UUID's start/end
	resultcode = Mod_FindUUIDLoc(&tempStart, &tempEnd, UUID);
	if(resultcode == FALSE){
		return FALSE; //Database error. That's no good.
	}
	
	//Check if SrcType is set to "PE"
	if(
		strcmp(FileType, "PE") == 0 &&
		strcmp(FileName, ":memory:") != 0
	){
		//Convert file offset to memory address
		tempStart = File_OffToPE(FilePath, tempStart);
		tempEnd = File_OffToPE(FilePath, tempEnd);
	}
	
	if(IsSrc){
		input->SrcStart = tempStart;
		input->SrcEnd = tempEnd;
	} else {
		input->Start = tempStart;
		input->End = tempEnd;
	}
	
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_MangleUUID
 *  Description:  Converts the given UUID into a path-safe name
 * =====================================================================================
 */
char * Mod_MangleUUID(const char *UUID)
{
	char *result = strdup(UUID);
	const char blacklist[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
		17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
		'<', '>', ':', '"', '/', '\\', '|', '?', '*', '\0'
	}; //This is technically a valid string. That makes things nice.
	char *ptr = result;
	
	ptr = strpbrk (UUID, blacklist);
	while (ptr != NULL){
		*ptr = '_';
		ptr = strpbrk (UUID, blacklist);
	}
	
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_Verify
 *  Description:  Calls various other functions to make sure mod is OK to install
 * =====================================================================================
 */
BOOL Mod_Verify(json_t *root)
{
	if (Mod_CheckCompat(root) == FALSE){
		return FALSE;
	}
	if (Mod_CheckConflict(root) == FALSE){
		return FALSE;
	}
	if (Mod_CheckDep(root) == FALSE){
		if(CURRERROR == errNOERR){
			Mod_CheckDepAlert(root);
		}
		return FALSE;
	}
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_Install
 *  Description:  Parses array of patches and adds/removes bytes depending on values.
 *                Patches support 6 operations, defined in manual.
 * =====================================================================================
 */
BOOL Mod_Install(json_t *root, const char *path)
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
	
	//Start SQL transaction (huge speedup!)
	sqlite3_exec(CURRDB, "BEGIN TRANSACTION", NULL, 0, NULL);
	
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
		char *Mode = JSON_GetStr(patchCurr, "Mode");
		struct ModSpace input = {0};
		char *FileName = JSON_GetStr(patchCurr, "File");
		
		//Change directory back to game's root in case it was changed.
		chdir(CONFIG.CURRDIR);
		ErrNo2ErrCode();
		if(CURRERROR != errNOERR){
			retval = FALSE;
			goto Mod_Install_NextPatch;
		}
		
		//Start file transaction
		if(!FileName || strcmp(FileName, "") == 0){
			AlertMsg(
				"'File' not defined for patch.",
				"JSON Error."
			);
			retval = FALSE;
			goto Mod_Install_NextPatch;
		}
/*		if(!File_TransactStart){
			char *msg;
			asprintf(&msg, 
				"\"%s\" is not the path to a valid file.",
				FileName
			);
			AlertMsg(msg, "File error");
			safe_free(msg);
			retval = FALSE;
			goto Mod_Install_NextPatch;
		}*/
		
		//Get patch mode
		if(!Mode || strcmp(Mode, "") == 0){
			//Mode was not found
			AlertMsg("`Mode` not defined for patch.", "JSON Error");
			retval = FALSE;
			goto Mod_Install_NextPatch;
		}
		
		//Fill input struct
		input = Mod_GetPatchInfo(patchCurr, path);
		if(input.Valid == FALSE){
			retval = FALSE;
			goto Mod_Install_NextPatch;
		}
		
		//Check variable condition
		if(
				Mod_PatchKeyExists(patchCurr, "ConditionVar", FALSE) &&
				Mod_PatchKeyExists(patchCurr, "ConditionOp", FALSE) &&
				Mod_PatchKeyExists(patchCurr, "ConditionValue", FALSE)
		){
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
				signed long CondVal = JSON_GetInt(patchCurr, "ConditionValue");
				CurrComp = Var_Compare(&CurrVar, CondOp, &CondVal, 4);
				
			}
			break;
			
			case uInt32:
			case uInt16:
			case uInt8:{
				unsigned long CondVal;
				char *CondValStr;
				CondValStr = JSON_GetStr(patchCurr, "ConditionValue");
				CondVal = strtoul(CondValStr, NULL, 0);
				
				CurrComp = Var_Compare(&CurrVar, CondOp, &CondVal, 4);
				
				safe_free(CondValStr);
			}
			break;
			
			case IEEE32:{
				float CondVal = (float)JSON_GetDouble(patchCurr, "ConditionValue");
				CurrComp = Var_Compare(&CurrVar, CondOp, &CondVal, 4);
			}
			break;
			case IEEE64:{
				double CondVal = JSON_GetDouble(patchCurr, "ConditionValue");
				CurrComp = Var_Compare(&CurrVar, CondOp, &CondVal, 8);
			}
			break;
			
			}
				
			//Go to next patch if condition is false and valid
			if(
				!CondVar ||
				!(strcmp(CondVar, "") == 0 ||
				CurrVar.type == INVALID) &&
				CurrComp == FALSE
			){
				safe_free(CondVar);
				safe_free(CondOp);
				AlertMsg("Skipping patch due to variable", "DEBUG");
				goto Mod_Install_NextPatch;
			}
			safe_free(CondVar);
			safe_free(CondOp);
		}

		///Apply patches
		//Replace
		if(strcmp(Mode, "Repl") == 0){
			retval = ModOp_Clear(&input, ModUUID) |
			ModOp_Add(&input, ModUUID);
		//Clear
		} else if(strcmp(Mode, "Clear") == 0){
			retval = ModOp_Clear(&input, ModUUID);
		//Add or Copy (Copy sets bytes beforehand to emulate Add)
		} else if(strcmp(Mode, "Add") == 0 || strcmp(Mode, "Copy") == 0){
			retval = ModOp_Add(&input, ModUUID);
		//Reserve
		} else if(strcmp(Mode, "Reserve") == 0){
			retval = ModOp_Reserve(&input, ModUUID);
		//Move
		} else if(strcmp(Mode, "Move") == 0){
			//This is gonna get kinda weird.
			//Make a new modspace representing the source chunk
			struct ModSpace srcInput = input;
			srcInput.Start = srcInput.SrcStart;
			srcInput.End = srcInput.SrcEnd;
			
			//Remove the source
			retval = ModOp_Clear(&srcInput, ModUUID);
			if(retval == FALSE){
				goto Mod_Install_NextPatch;
			}
			
			//Add new one in new location
			retval = ModOp_Add(&input, ModUUID); //Add to Dest
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
			asprintf(&varCurr.UUID, "Start.%s", ModUUID); 
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
				int CurrVarLen;
				int CurrVarLoc;
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
				//DO NOT free CurrInput.ID (still same as input.ID)
				
				asprintf(&CurrInput.ID, "%s/%d", input.ID, j);
				
				CurrInput.Start += CurrVarLoc;
				CurrInput.End = CurrInput.Start + CurrVarLen;
				CurrInput.Len = CurrVarLen;
				
				CurrInput.Bytes = malloc(CurrVarLen);
				memcpy(&CurrInput.Bytes, &CurrVar.raw[0], CurrVarLen);
				
				CurrInput.Valid = TRUE;
				
				//Splice value in
				ModOp_Clear(&CurrInput, ModUUID);
				ModOp_Add(&CurrInput, ModUUID);

				safe_free(CurrInput.ID);
				
			MiniPatch_CleanUp:
				safe_free(CurrVarStr);
			}
		}
		
	Mod_Install_NextPatch:
		safe_free(input.Bytes);
		safe_free(input.ID);
		safe_free(FileName);
		ProgDialog_Update(ProgDialog, 1);
		
		if(retval == FALSE || CURRERROR != errNOERR){
			char *msg = NULL;
			asprintf(&msg, "Mod configuration error on patch #%d", i + 1);
			AlertMsg(msg, "JSON error");
			safe_free(msg);
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
	Mod_AddToDB(root, path);
	safe_free(ModUUID);
	
	if(CURRERROR != errNOERR){retval = FALSE;}
	ProgDialog_Kill(ProgDialog);
	
	sqlite3_exec(CURRDB, "END TRANSACTION", NULL, 0, NULL);
	//On failure, uninstall (can't rollback sql; files are still written to.)
	/*if(retval == FALSE){
		Mod_Uninstall(ModUUID);
	}*/
	
	return retval;
}

//Add mod to database
BOOL Mod_AddToDB(json_t *root, const char *path){
	sqlite3_stmt *command;
	char *uuid = JSON_GetStr(root, "UUID"); 
	char *name = JSON_GetStr(root, "Name");
	char *desc = JSON_GetStr(root, "Desc");
	char *auth = JSON_GetStr(root, "Auth");
	char *date = JSON_GetStr(root, "Date");
	char *cat = JSON_GetStr(root, "Cat");
	int ver = JSON_GetInt(root, "Ver");
	
	const char *query = "INSERT INTO Mods "
		"('UUID', 'Name', 'Desc', 'Auth', 'Ver', 'Date', 'Cat', 'Path') VALUES "
		"(?, ?, ?, ?, ?, ?, ?, ?);";
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, uuid, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 2, name, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 3, desc, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 4, auth, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 5, ver) |
		sqlite3_bind_text(command, 6, date, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 7, cat, -1, SQLITE_STATIC) |
		sqlite3_bind_text(command, 8, path, -1, SQLITE_STATIC)
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
	const char *query = "SELECT EXISTS(SELECT * FROM Dependencies "
		"JOIN Mods ON Dependencies.ParentUUID = Mods.UUID "
		"WHERE Dependencies.ChildUUID = ?)";
		
	CURRERROR = errNOERR;
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
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
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
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

//Remove entry for PatchUUID. Also un-claims previous space.
BOOL Mod_Uninstall_Remove(const char *PatchUUID)
{
	sqlite3_stmt *command;
	const char *query = "DELETE FROM Spaces WHERE ID = ? AND Ver = ?;";
	CURRERROR = errNOERR;

	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC) |
		sqlite3_bind_int(command, 2, Mod_GetVerCount(PatchUUID))
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	command = NULL;
	
	Mod_UnClaimSpace(PatchUUID);
	return TRUE;
}

// Remove entry for PatchUUID and rename patch PatchUUID~oldx with highest 'x'
// to PatchUUID

// Obsoleted; old names no longer exist. Use Mod_Uninstall_Remove instead.

/*BOOL Mod_Uninstall_Restore(const char *PatchUUID)
{
	char *oldID;
	int oldCount;

	Mod_Uninstall_Remove(PatchUUID); //May or may not exist to begin with
	if(Mod_SpaceExists(PatchUUID)){
		return FALSE;
	}

	//asprintf(&oldID, "%s~old", PatchUUID);
	
	oldCount = Mod_GetOldCount(PatchUUID);
	
	if(oldCount == -1){
		//Nothing to restore
		return TRUE;
	}
	
	asprintf(&oldID, "%s~old%d", PatchUUID, oldCount);
	
	//Remove any instances of "~old" in newest ID
	if(!Mod_RenameSpace(oldID, PatchUUID)){
		CURRERROR = errCRIT_DBASE;
		safe_free(oldID);
		return FALSE;
	}

	safe_free(oldID);
	return TRUE;
}*/

//Remove the MERGE space. Other space will automatically be usable.
BOOL ModOp_UnMerge(json_t *row)
{
	//Just do a restore on it
	char *PatchUUID = JSON_GetStr(row, "ID");
//	if(!Mod_Uninstall_Restore(PatchUUID)){
	if(!Mod_Uninstall_Remove(PatchUUID)){
		return FALSE;
	}

	safe_free(PatchUUID);
	return TRUE;
}

//Remove spaces created by split and restore split space to original state
BOOL ModOp_UnSplit(json_t *row)
{
	char *PatchUUID = JSON_GetStr(row, "ID");
//	char *HeadUUID = JSON_GetStr(row, "Start");
//	char *TailUUID = JSON_GetStr(row, "End");
	BOOL retval = FALSE;
	
	if(
//		Mod_Uninstall_Remove(HeadUUID) &&
//		Mod_Uninstall_Remove(TailUUID) &&
	//	Mod_Uninstall_Restore(PatchUUID)
		Mod_Uninstall_Remove(PatchUUID)
	){
		retval = TRUE;
	}
	
	safe_free(PatchUUID);
//	safe_free(HeadUUID);
//	safe_free(TailUUID);
	return retval;
}

//Write back the old bytes and then remove the space.
//Used for Add and Clear
BOOL ModOp_UnSpace(json_t *row)
{
	int filehandle, offset, datalen;
	unsigned char *bytes;
	char *ByteStr, *PatchUUID;
	char *FileName = File_GetName(JSON_GetInt(row, "File"));
	char *FilePath = NULL;
	CURRERROR = errNOERR;
	
	//Open file handle
	if(FileName == NULL){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
	filehandle = File_OpenSafe(FilePath, _O_BINARY|_O_RDWR);

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
			sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
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
	
	//Restore the backup if it exists and the target file doesn't
	{
		char *BakPath = NULL;
		asprintf(&BakPath, "%s/BACKUP/%s", CONFIG.CURRDIR, FileName);
		
		if(
			File_Exists(BakPath, FALSE, FALSE) &&
			!File_Exists(FilePath, FALSE, FALSE) &&
			CURRERROR == errWNG_READONLY
		){
			File_MovTree(BakPath, FilePath);
		}
		
		CURRERROR = errNOERR; //Reset error.
		safe_free(BakPath);
	}
	
	//Write back data
	File_WriteBytes(filehandle, offset, bytes, datalen);
	
	safe_free(bytes);
	safe_free(FilePath);
	safe_free(FileName);
	close(filehandle);
	
	//Remove the old bytes
	{
		sqlite3_stmt *command;
		const char *query = "DELETE FROM Revert WHERE PatchUUID = ?;";
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		command = NULL;
	}
	
	//Remove the space
//	Mod_Uninstall_Restore(PatchUUID);
	Mod_Uninstall_Remove(PatchUUID);
	
	//Unclaim any overlapping spaces
	{
		sqlite3_stmt *command;
		const char *query = 
			"SELECT a.ID, a.Start, a.End FROM Spaces a "
			"LEFT OUTER JOIN Spaces b ON a.ID = b.ID AND a.Ver < b.Ver "
			"WHERE b.ID IS NULL AND "
			"(a.End >= ? AND a.Start <= ?) AND "
			"a.File = ? "
			"ORDER BY a.Start ASC";
		json_t *out, *patch;
		size_t i;
		int start, end, file;
		start = JSON_GetInt(row, "Start");
		end = JSON_GetInt(row, "End");
		file = JSON_GetInt(row, "File");
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__,
			sqlite3_bind_int(command, 1, start) |
			sqlite3_bind_int(command, 2, end) |
			sqlite3_bind_int(command, 3, file)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		
		out = SQL_GetJSON(command);
		if(!json_is_array(out)){
			//There's no spaces. Whatever.
			json_decref(out);
			return TRUE;
		}
		sqlite3_finalize(command);

		// Un-claim.
		json_array_foreach(out, i, patch){
			char *ID = JSON_GetStr(patch, "ID");
			Mod_UnClaimSpace(ID);
			safe_free(ID);
		}
		
		json_decref(out);
	}

	return TRUE;
}

BOOL Mod_Uninstall(const char *ModUUID)
{
	BOOL retval = TRUE;
//	unsigned int i;
	
	//Define progress dialog (handle is shell-specific)
//	#ifdef _WIN32
//	HWND ProgDialog;
//	#endif
	
	CURRERROR = errNOERR;
	
	chdir(CONFIG.CURRDIR); //In case path was changed during installation or w/e.
	
	//Check for dependencies
	if(Mod_FindDep(ModUUID) == TRUE){
		Mod_FindDepAlert(ModUUID);
		return FALSE;
	};
	
	//Init progress box
//	ProgDialog = ProgDialog_Init(modCount, "Uninstalling Mod...");


	//Select every patch created by current mod
	//PROBLEM: This quickly becomes invalid. Eugh.
/*	{
		sqlite3_stmt *command;
		const char *query = "SELECT * FROM Spaces WHERE Mod = ? "
		                    "ORDER BY ROWID Desc";
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			retval = FALSE;
			goto Mod_Uninstall_Cleanup;
		}
		
		out = SQL_GetJSON(command);
		sqlite3_finalize(command);
		command = NULL;
	}
	
	//Loop through patches
	json_array_foreach(out, i, row){

		char *SpaceType = JSON_GetStr(row, "Type");

		if(
			strcmp(SpaceType, "Add") == 0 ||
			strcmp(SpaceType, "Clear") == 0
		){
			if(!ModOp_UnSpace(row)){
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}

		} else if (
			strcmp(SpaceType, "Merge") == 0
		){
			if(!ModOp_UnMerge(row)){
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
		} else if (
			strcmp(SpaceType, "Split") == 0
		){
			if(!ModOp_UnSplit(row)){
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
		}

		safe_free(SpaceType);
	}
	json_decref(out);
	out = NULL;
*/
	
	//How many patches, Carmine?
/*	{
		sqlite3_stmt *command;
		const char *query = "SELECT COUNT(*) FROM Spaces WHERE Mod = ?";
		
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			retval = FALSE;
			goto Mod_Uninstall_Cleanup;
		}
		
		i = SQL_GetNum(command);
		sqlite3_finalize(command);
		command = NULL;
	}*/
	//(The answer may or may not be 8.)
	
	//for(; i > 0; i++){
	while(TRUE){
		// I know getting a new SQL query for each result is inefficient.
		// I don't care. (But honestly we probably could replace this with
		// a json for-loop now that I think about it.)
		json_t *out, *row;
		sqlite3_stmt *command;
		const char *query = "SELECT * FROM Spaces WHERE Mod = ? "
		                    "ORDER BY ROWID Desc";
		char *SpaceType = NULL;
		//Get the last row
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			retval = FALSE;
			goto Mod_Uninstall_Cleanup;
		}
		
		out = SQL_GetJSON(command);
		sqlite3_finalize(command);
		
		row = json_array_get(out, 0);
		SpaceType = JSON_GetStr(row, "Type");

		if(strcmp(SpaceType, "") == 0){
			// No more spaces
			safe_free(SpaceType);
			json_decref(out);
			break;
		}
		
		//Perform space-specific remove operation
		if(
			strcmp(SpaceType, "Add") == 0 ||
			strcmp(SpaceType, "Clear") == 0
		){
			if(!ModOp_UnSpace(row)){
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}

		} else if (
			strcmp(SpaceType, "Merge") == 0
		){
			if(!ModOp_UnMerge(row)){
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}

		} else if (
			strcmp(SpaceType, "Split") == 0
		){
			if(!ModOp_UnSplit(row)){
				retval = FALSE;
				goto Mod_Uninstall_Cleanup;
			}
		}

		safe_free(SpaceType);
		json_decref(out);
	}
	
	//Mod has now been uninstalled.
	
	//Unmark used spaces
	{
		sqlite3_stmt *command;
		//Clear UsedBy if equal to UUID of current mod
		const char *query = "UPDATE Spaces SET UsedBy = NULL WHERE UsedBy = ?;";
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
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
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			retval = FALSE;
			goto Mod_Uninstall_Cleanup;
		}
		command = NULL;
	}
	
	//Remove mod variables
	Var_ClearEntry(ModUUID);
	
	//Increment progress bar
//	ProgDialog_Update(ProgDialog, 1);
	
Mod_Uninstall_Cleanup:
	//Kill progress box
//	ProgDialog_Kill(ProgDialog);
	return retval;
}

// Uninstall everything below the given UUID
// Returns double-null-terminated list of the path of
// every uninstalled mod except for the input.
char * Mod_UninstallSeries(const char *UUID)
{
	unsigned int modCount, i, modStop;
	char *retList, *retPtr;
	unsigned int retListSize = 0, retListCap = 8;
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
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
		
		modStop = SQL_GetNum(command);
		if(SQL_HandleErrors(__LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
	}
	
	//Allocate list
	retList = malloc(retListCap);
	retPtr = retList;
	
	//Loop through all mods
	for(i = modCount; i >= modStop; i--){
		char *CurrUUID = NULL;
		char *CurrPath = NULL;
		sqlite3_stmt *command;
		const char *query1 = "SELECT UUID FROM Mods WHERE RowID = ?;";
		const char *query2 = "SELECT Path FROM Mods WHERE UUID = ?";
		
		//Get UUID of current mod
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_int(command, 1, i)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			//Oh no.
			safe_free(retList);
			retList = strdup("");
			return retList;
		}
		//Get UUID
		CurrUUID = SQL_GetStr(command);
		//Finalize
		sqlite3_finalize(command);
		command = NULL;

		//Get path
		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, CurrUUID, -1, SQLITE_STATIC)
		) != 0 ){
			CURRERROR = errCRIT_DBASE;
			//Oh no. This is worse.
			safe_free(retList);
			safe_free(CurrUUID);
			retList = strdup("");
			return retList;
		}

		CurrPath = SQL_GetStr(command);
		sqlite3_finalize(command);
		command = NULL;

		//Uninstall!
		Mod_Uninstall(CurrUUID);
		
		//Record in list
		if(i == modStop){
			//Don't record first one.
			safe_free(CurrUUID);
			safe_free(CurrPath);
			continue;
		}
		while(retListSize + strlen(CurrPath) + 1 > retListCap){
			retListCap *= 2;
			retList = realloc(retList, retListCap);
			retPtr = retList + retListSize;
		}
		strcpy(retPtr, CurrPath);
		retListSize += strlen(CurrPath) + 1;

		safe_free(CurrUUID);
		safe_free(CurrPath);
	}
	
	//Throw on final null terminator
	//retListSize += 1;
	while(retListSize + 1 > retListCap){
		retListCap *= 2;
		retList = realloc(retList, retListCap);
	}
	retPtr = retList + retListSize;
	*retPtr = '\0';

	return retList;
}

// Install every mod in the given double-null-terminated list in order
BOOL Mod_InstallSeries(const char *ModList)
{
	const char *modPath = ModList;
	BOOL retval = TRUE;
	//Begin SQL transaction
	//sqlite3_exec(CURRDB, "BEGIN TRANSACTION", NULL, 0, NULL);
	
	while(*modPath != '\0' && retval == TRUE){
		//For each string...
		char *jsonPath = NULL;
		json_t *root;
		
		//Retreive JSON
		asprintf(&jsonPath, "%s/%s", modPath, "info.json");
		root = JSON_Load(jsonPath);
		
		//Install mod
		//TODO: Check return value of this and act accordingly
		retval = Mod_Install(root, modPath);

		//Merge clear spaces
/*		if(retval == TRUE){
			Mod_MergeFreeSpace();
		}*/

		//Remove all-clear files

		//Increment pointer
		if(retval == TRUE){
			modPath += strlen(modPath) + 1;
		}
		safe_free(jsonPath);
		json_decref(root);
	}
	
	//DO NOT rollback SQL transaction. If you do, files on disk will
	//be in a strange half-written state. Instead, commit and then
	//immediately uninstall.
	//sqlite3_exec(CURRDB, "COMMIT TRANSACTION", NULL, 0, NULL);
	if(retval == FALSE){
		Mod_Uninstall(modPath);
	}
	return TRUE;
}

//Uninstall every mod up to and including ModUUID, then reinstall in order
BOOL Mod_Reinstall(const char *ModUUID)
{
	char *retList = NULL;
	retList = Mod_UninstallSeries(ModUUID);
	
	//Install ModUUID
	{
		char *modDir = Mod_MangleUUID(ModUUID);
		char *modPath = NULL;
		char *jsonPath = NULL;
		json_t *root;
		
		//Retreive JSON
		asprintf(&modPath, "%s/mods/%s", CONFIG.CURRDIR, modDir);
		asprintf(&jsonPath, "%s/%s", modPath, "info.json");
		root = JSON_Load(jsonPath);
		
		//Install mod
		Mod_Install(root, modPath);
		
		//Deallocate crap
		safe_free(modPath);
		safe_free(modDir);
		safe_free(jsonPath);
		json_decref(root);
	}
	
	//Install rest
	Mod_InstallSeries(retList);
	safe_free(retList);
	return TRUE;
}

///Profile stuff
//Make a profile config file from profile data
void Profile_Save(
	const char *profile,
	const char *game,
	const char *gameConf,
	int checksum,
	const char *runpath,
	const char *gamever
){
	json_t *Config = json_pack(
		"{s:s, s:s, s:i, s:s, s:s}",
		"GamePath", game,
		"MetadataPath", gameConf,
		"Checksum", checksum,
		"RunPath", runpath,
		"GameVer", gamever
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
		LocalConfig->RUNPATH,
		LocalConfig->GAMEVER
	);
	return;
}

struct ProgConfig * Profile_Load(char *fpath)
{
	struct ProgConfig *LocalConfig;
	json_t *GameCfg, *Profile;
//	char *GameEXE;
	
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
	LocalConfig->GAMEVER = JSON_GetStr(Profile, "GameVer");
	
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
	//Get game UUID
	LocalConfig->GAMEUUID = JSON_GetStr(GameCfg, "GameUUID");
	
	//Check if checksum is sane
/*	chdir(LocalConfig->CURRDIR);
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
		safe_free(msg);
		
		//Clear game path and checksum
		LocalConfig->CHECKSUM = 0; //This might be dangerous. Let's see.
		safe_free(LocalConfig->CURRDIR);
		chdir(CONFIG.PROGDIR);
	}*/
	
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

char * Profile_GetGameVerID(const char *cfgpath)
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
		), "Name"
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

char * Profile_GetGameUUID(const char *cfgpath)
{
	char *GameName;
	json_t *CurrCfg;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return strdup("");
	}
	
	GameName = JSON_GetStr(CurrCfg, "GameUUID");
	
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
	safe_free(LocalConfig->GAMEVER);
	safe_free(LocalConfig->GAMEUUID);
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
	chdir(CONFIG.CURRDIR);
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_open("mods.db", &CURRDB)
	) != 0 || SQL_HandleErrors(__LINE__, 
		sqlite3_exec(CURRDB, 
			"CREATE TABLE IF NOT EXISTS 'Spaces'( "
			"`ID`   	TEXT NOT NULL,"
			"`Ver`          INTEGER NOT NULL,"
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
			"`Cat` 	TEXT,"
			"`Path` TEXT);",
			NULL, NULL, NULL
		) | sqlite3_exec(CURRDB,
			"CREATE TABLE IF NOT EXISTS `Dependencies` ("
			"`ParentUUID`	TEXT NOT NULL,"
			"`ChildUUID` 	TEXT NOT NULL);",
			NULL, NULL, NULL
		) | sqlite3_exec(CURRDB,
			"CREATE TABLE IF NOT EXISTS `Revert` ("
			"`PatchUUID`	TEXT NOT NULL,"
			"`Ver`          INTEGER NOT NULL,"
			"`OldBytes` 	TEXT NOT NULL);",
			NULL, NULL, NULL
		) | sqlite3_exec(CURRDB, 
			"CREATE TABLE IF NOT EXISTS `Files` ("
			"`ID`  	INTEGER NOT NULL UNIQUE,"
			"`Path`	TEXT NOT NULL);",
			NULL, NULL, NULL
		) | sqlite3_exec(CURRDB,
			"CREATE TABLE IF NOT EXISTS `Variables` ("
			"`UUID`       TEXT NOT NULL UNIQUE,"
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
	json_t *out, *row;
	size_t i;
	int spaceCount;

	const char *query1 = "SELECT EXISTS(SELECT * FROM Spaces)";
	const char *query2 = "SELECT * FROM Files";
	const char *query3 = "UPDATE Spaces SET Type = 'Add' WHERE ID = ? AND Type = 'Clear'";
	sqlite3_stmt *command;

	#ifdef _WIN32
	HWND ProgDialog;
	#endif

	CURRERROR = errNOERR;

	//Don't populate if we don't need to
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
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
	
	//Actually return if there's something
	if (spaceCount != 0){return TRUE;}
	
	//To decrease disk I/O and increase speed
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_exec(CURRDB, "BEGIN TRANSACTION", NULL, NULL, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	//Create a CLEAR space and file entry for the memory "file".
	File_MakeEntry(":memory:");

	out = json_object_get(GameCfg, "KnownSpaces");
	ProgDialog = ProgDialog_Init(
		json_array_size(out),
		"Setting up mod database..."
	);

	// Reserve spaces for each defined function
	json_array_foreach(out, i, row){
		
		struct ModSpace NewSpc = {0};
		char *FileName = JSON_GetStr(row, "File");
		char *FilePath = NULL;

		asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
		
		NewSpc.Start = JSON_GetInt(row, "Start");
		NewSpc.End = JSON_GetInt(row, "End") - 1;
		NewSpc.ID = JSON_GetStr(row, "Name");
		NewSpc.FileID = File_GetID(FileName);

		// Turns out my idc2json converter outputs PE offsets,
		// but it's stored as file offsets. Let's fix that.
		// (It also doesn't output any unnamed subroutines.
		//  That's a bad thing.)
		NewSpc.Start = File_PEToOff(FilePath, NewSpc.Start);
		NewSpc.End = File_PEToOff(FilePath, NewSpc.End);
		NewSpc.Len = NewSpc.End - NewSpc.Start;
		NewSpc.Valid = TRUE;
		
		ModOp_Reserve(&NewSpc, "MODLOADER@invisibleup");
		ErrCracker(CURRERROR);
		
		safe_free(NewSpc.ID);
		safe_free(FileName);
		safe_free(FilePath);
		ProgDialog_Update(ProgDialog, 1);
	}
	json_decref(out);

	// Oh, also, change those big ol' CLEAR spaces to ADD spaces.
	// You know, before we overwrite something important.
	if(SQL_HandleErrors(__LINE__, 
		sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	out = SQL_GetJSON(command);
	sqlite3_finalize(command);

	json_array_foreach(out, i, row){
		char *FileName = JSON_GetStr(row, "Path");
		char *FilePatch = NULL;
		asprintf(&FilePatch, "Base.%s", FileName);

		if(SQL_HandleErrors(__LINE__, 
			sqlite3_prepare_v2(CURRDB, query3, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__LINE__, 
			sqlite3_bind_text(command, 1, FilePatch, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

	}


	if(SQL_HandleErrors(__LINE__, 
		sqlite3_exec(CURRDB, "COMMIT TRANSACTION", NULL, NULL, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	ProgDialog_Kill(ProgDialog);

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