/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This contains the main() function and some random helper functions that
// fit nowhere else.

#include "includes.h"              // LOCAL: All includes
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages

// Version info
const long PROGVERSION =           // Program version in format 0x00MMmmbb
	PROGVERSION_BUGFIX + 
	(PROGVERSION_MINOR*0x100) +
	(PROGVERSION_MAJOR*0x10000);
// Name of program's primary author and website
const char PROGAUTHOR[] = "InvisibleUp";
const char PROGSITE[] = "http://github.com/InvisibleUp/SrModLdr/";

// Globals
struct ProgConfig CONFIG = {0};    // Global program configuration
sqlite3 *CURRDB = NULL;            // Current database holding patches
enum errCode CURRERROR = errNOERR; // Current error state
#ifdef HAVE_WINDOWS_H
HINSTANCE CURRINSTANCE = NULL;     // (Win32) Current instance
#endif

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
	errno = 0;
    return;
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
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
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
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_reset(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
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
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
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
	if(SQL_HandleErrors(__FILE__, __LINE__, 
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

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ForceStrNumeric
 *  Description:  Drops all non-numeric characters from a string except the first
 *                decimal marker.
 * =====================================================================================
 */
char * ForceStrNumeric(const char *input)
{
	unsigned int i;
	BOOL decimalFound = FALSE;
	char *result = malloc(strlen(input)+1);
	char *resultAddr = result;
	if (!result) {
		CURRERROR = errCRIT_MALLOC;
		return NULL;
	}
	
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
 *         Name:  Hex2Bytes
 *  Description:  Converts a hexadecimal string to an array of bytes.
 * =====================================================================================
 */
unsigned char * Hex2Bytes(const char *hexstring, int *len)
{
	int i = 0;
	//Allocate space for every byte + null byte (just in case)
	unsigned char *result = malloc((strlen(hexstring) + 2 / 2));
	char currByte[3];
	const char *currBytePtr = hexstring;
	if (!result) {
		CURRERROR = errCRIT_MALLOC;
		return NULL;
	}
	
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

	val = calloc(len*4, sizeof(char)); // Overshooting
	if (!val) {
		CURRERROR = errCRIT_MALLOC;
		return NULL;
	}
	for(i = 0; i < len; i++){
		// Not using asprintf for speed reasons
		char temp[8];
		snprintf(temp, 7, "%02X", bytes[i]);
		strncat(val, temp, len-strlen(val));
	}
	return val;
}

#ifndef filesize
#ifdef HAVE_WINDOWS_H
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  filesize
 *  Description:  Drop-in replacement for filesize in io.h
 * =====================================================================================
 */
long filesize(const char *filename){
	WIN32_FILE_ATTRIBUTE_DATA fad;
//	LARGE_INTEGER size;

    if (!GetFileAttributesEx(filename, GetFileExInfoStandard, &fad)){
		int temp = GetLastError();
        return -1;
	}

//   size.HighPart = fad.nFileSizeHigh;
//    size.LowPart = fad.nFileSizeLow;
//    return size.QuadPart;
	return fad.nFileSizeLow; // This won't be a problem for files < 4 GB. Which is all of them.
}
#elif HAVE_STAT_H
long filesize(const char *filename){
	struct stat out;
	stat(filename, &out);
	ErrNo2ErrCode();
	return out.st_size;
}
#else
	#error "No filesize function found!"
#endif
#endif

// That's right, x86 is little-endian. We need a memcpy() that takes
// this into account. 
// From http://stackoverflow.com/questions/2242498/c-memcpy-in-reverse/2242531
void memcpy_rev(unsigned char *dst, const unsigned char *src, size_t n)
{
    size_t i;

    for (i=0; i < n; ++i){
        dst[n-1-i] = src[i];
	}

}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_Load
 *  Description:  Opens the database and creates the tables if they don't exist.
 *
 *                In the future this will need to handle upgrading the DB schema to
 *                new versions.
 * =====================================================================================
 */
BOOL SQL_Load(){
    char *DBPath = NULL;
	CURRERROR = errNOERR;
	//chdir(CONFIG.CURRDIR);
    
    asprintf(&DBPath, "%s/mods.db", CONFIG.CURRDIR);
    
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		//sqlite3_open(DBPath, &CURRDB) |
        sqlite3_open(":memory:", &CURRDB) |
        sqlite3_extended_result_codes(CURRDB, 1)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__,                       
		sqlite3_exec(CURRDB, 
            "PRAGMA application_id = 2695796694;" // Randomly generated number
            "PRAGMA user_version = 1;"
            "PRAGMA journal_mode=MEMORY;" // If it crashes, you're in a pickle anyways...
            "PRAGMA mmap_size=16777216;"
			"CREATE TABLE IF NOT EXISTS 'Spaces'( "
			"`ID`           	TEXT NOT NULL,"
			"`Version`  		INTEGER NOT NULL,"
			"`Type`         	TEXT NOT NULL,"
			"`File`         	INTEGER NOT NULL,"
			"`Mod`          	TEXT,"
			"`PatchID`          TEXT,"
			"`Start`        	INTEGER NOT NULL,"
			"`End`          	INTEGER NOT NULL,"
			"`Len`          	INTEGER,"
			"`UsedBy`       	TEXT );"
			"CREATE TABLE IF NOT EXISTS `Mods` ("
			"`UUID`				TEXT NOT NULL UNIQUE PRIMARY KEY,"
			"`Name`				TEXT NOT NULL,"
			"`Info`         	TEXT,"
			"`Author`         	TEXT,"
			"`Version`        	INTEGER NOT NULL,"
			"`Date`         	TEXT,"
			"`Category`        	TEXT,"
			"`Path`         	TEXT);"
			"CREATE TABLE IF NOT EXISTS `Dependencies` ("
			"`ParentUUID`     	TEXT NOT NULL,"
			"`ChildUUID`      	TEXT NOT NULL);"
			"CREATE TABLE IF NOT EXISTS `Revert` ("
			"`PatchUUID`		TEXT NOT NULL UNIQUE," // NOT Space ID
			"`Start`            INTEGER NOT NULL,"
			"`OldBytes`			TEXT NOT NULL);"
			"CREATE TABLE IF NOT EXISTS `Files` ("
			"`ID`				INTEGER NOT NULL UNIQUE,"
			"`Path`				TEXT NOT NULL);"
			"CREATE TABLE IF NOT EXISTS `Variables` ("
			"`UUID`				TEXT NOT NULL UNIQUE,"
			"`Mod`				TEXT NOT NULL,"
			"`Type`				TEXT NOT NULL,"
			"`PublicType`   	TEXT,"
			"`Info`		    	TEXT,"
			"`Value`        	BLOB,"
			"`Persist`	    	INTEGER);"
			"CREATE TABLE IF NOT EXISTS `VarList` ("
			"`Var`          	TEXT NOT NULL,"
			"`Number`       	INTEGER NOT NULL,"
			"`Label`        	TEXT NOT NULL);"
			"CREATE TABLE IF NOT EXISTS `VarRepatch` ("
			"`Var`              TEXT NOT NULL,"
			"`ModPath`          TEXT NOT NULL,"
			"`Patch`            INTEGER NOT NULL,"
			"`OldVal`           INTEGER NOT NULL);",
            NULL, NULL, NULL
		)
	) != 0){
        safe_free(DBPath);
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	safe_free(DBPath);
	
	//Add mod loader version var
	{
		struct VarValue varCurr;
		
		//Check if variable exists
		varCurr = Var_GetValue_SQL("Version.MODLOADER@invisibleup");
		if(varCurr.type == INVALID){
			//Variable does not exist; make it!
			varCurr.type = uInt32;
			varCurr.UUID = strdup("Version.MODLOADER@invisibleup");
			varCurr.desc = strdup("Version of mod loader");
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
		Var_Destructor(&varCurr);
	}
	
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_Populate
 *  Description:  If no spaces are defined, runs through the game config and creates
 *                an ADD space for each and every space defined. This is likely to
 *                be at least a few hundred spaces, if not more.
 * =====================================================================================
 */
BOOL SQL_Populate(json_t *GameCfg)
{
	json_t *out, *row;
	size_t i;
	int spaceCount;
	char *ModDir = NULL;

	const char *query1 = "SELECT EXISTS(SELECT * FROM Spaces)";
	//const char *query2 = "SELECT * FROM Files";
	//const char *query3 = "UPDATE Spaces SET Type = 'Add' WHERE ID = ? AND Type = 'Clear'";
	const char *query4 = "UPDATE Spaces SET Type = 'Add' WHERE "
	                     "Mod = 'MODLOADER@invisibleup' AND Type = 'Clear'";
	sqlite3_stmt *command;

	ProgDialog_Handle ProgDialog;

	if (CURRERROR == errNOERR) { ErrNo2ErrCode(); }
	if (CURRERROR != errNOERR) { return FALSE; }

	CURRERROR = errNOERR;
	asprintf(&ModDir, "%s/mods", CONFIG.CURRDIR);

	//Don't populate if we don't need to
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	spaceCount = SQL_GetNum(command);
	if(CURRERROR != errNOERR){return FALSE;}
	
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	command = NULL;
	if(spaceCount != 0){return TRUE;}
	
	// To decrease disk I/O and increase speed
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_exec(CURRDB, "BEGIN TRANSACTION", NULL, NULL, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	// Create a CLEAR space and file entry for the memory "file".
	File_MakeEntry(":memory:");

	out = json_object_get(GameCfg, "KnownSpaces");
	ProgDialog = ProgDialog_Init(
		json_array_size(out),
		"Setting up mod database..."
	);

	// Create "mods" folder (Windows 7 needs this. 2000 doesn't...)
    // Might throw error, but it's platform-specific and likely "file exists"
	mkdir(ModDir); 
	safe_free(ModDir);

	// Reserve spaces for each defined function
	json_array_foreach(out, i, row){
		
		struct ModSpace NewSpc = {0};
		char *FileName = JSON_GetStr(row, "File");
		char *FilePath = NULL;

		asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
		
		NewSpc.Start = JSON_GetuInt(row, "Start");
		NewSpc.End = JSON_GetuInt(row, "End");
		NewSpc.ID = JSON_GetStr(row, "Name");
		NewSpc.FileID = File_GetID(FileName);
		asprintf(&NewSpc.PatchID, "%s.MODLOADER@invisibleup", NewSpc.ID);

		// Turns out my idc2json converter outputs PE offsets,
		// but it's stored as file offsets. Let's fix that.
		// (It also doesn't output any unnamed subroutines.
		//  Can't do much about that. Not that we should...)
		NewSpc.Start = File_PEToOff(FilePath, NewSpc.Start);
		NewSpc.End = File_PEToOff(FilePath, NewSpc.End);
		NewSpc.Len = NewSpc.End - NewSpc.Start;
		NewSpc.Valid = TRUE;
		
		ModOp_Reserve(&NewSpc, "MODLOADER@invisibleup");
		if (CURRERROR == errNOERR) {
			ErrNo2ErrCode();
		}
		if (CURRERROR != errNOERR){
			ErrCracker(CURRERROR);
			safe_free(NewSpc.Bytes);
			safe_free(NewSpc.ID);
			safe_free(FileName);
			safe_free(FilePath);
			safe_free(NewSpc.PatchID);
			json_decref(out);
			ProgDialog_Kill(ProgDialog);
			return FALSE;
		}

		// Create vars for patch location
		{
			struct VarValue varCurr;
			
			// Start
			varCurr.type = uInt32Pointer;
			asprintf(&varCurr.UUID, "Start.%s", NewSpc.PatchID); 
			varCurr.desc = strdup("Start byte of patch.");
			varCurr.publicType = strdup("");
			varCurr.mod = strdup("MODLOADER@invisibleup");
			varCurr.persist = FALSE;
			varCurr.uInt32 = File_OffToPE(FilePath, NewSpc.Start);

			Var_MakeEntry(varCurr);
			Var_Destructor(&varCurr);

			// End
			varCurr.type = uInt32Pointer;
			asprintf(&varCurr.UUID, "End.%s", NewSpc.PatchID); 
			varCurr.desc = strdup("End byte of patch.");
			varCurr.publicType = strdup("");
			varCurr.mod = strdup("MODLOADER@invisibleup");
			varCurr.persist = FALSE;
			varCurr.uInt32 = File_OffToPE(FilePath, NewSpc.End);

			Var_MakeEntry(varCurr);
			Var_Destructor(&varCurr);
		}
		
		safe_free(NewSpc.Bytes);
		safe_free(NewSpc.ID);
		safe_free(NewSpc.PatchID);
		safe_free(FileName);
		safe_free(FilePath);
		ProgDialog_Update(ProgDialog, 1);
	}
	//json_decref(out);

	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_exec(CURRDB, query4, NULL, NULL, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}


	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_exec(CURRDB, "COMMIT TRANSACTION", NULL, NULL, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	ProgDialog_Kill(ProgDialog);

	return TRUE;
}

#ifdef HAVE_WINDOWS_H
/*
* ===  FUNCTION  ======================================================================
*         Name:  CommandLineToArgvA
*  Description:  http://stackoverflow.com/a/4023686
*                Takes the Win32 command line inputs and converts it to an argv
*                formatted string so we can thunk WinMain() to main()
* =====================================================================================
*/
LPSTR* CommandLineToArgvA(LPSTR lpCmdLine, INT *pNumArgs)
{
	int retval;
	retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, NULL, 0);
	if (!SUCCEEDED(retval))
		return NULL;

	LPWSTR lpWideCharStr = (LPWSTR)malloc(retval * sizeof(WCHAR));
	if (lpWideCharStr == NULL)
		return NULL;

	retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, lpWideCharStr, retval);
	if (!SUCCEEDED(retval))
	{
		free(lpWideCharStr);
		return NULL;
	}

	int numArgs;
	LPWSTR* args;
	args = CommandLineToArgvW(lpWideCharStr, &numArgs);
	free(lpWideCharStr);
	if (args == NULL)
		return NULL;

	int storage = numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++i)
	{
		BOOL lpUsedDefaultChar = FALSE;
		retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, NULL, 0, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval))
		{
			LocalFree(args);
			return NULL;
		}

		storage += retval;
	}

	LPSTR* result = (LPSTR*)LocalAlloc(LMEM_FIXED, storage);
	if (result == NULL)
	{
		LocalFree(args);
		return NULL;
	}

	int bufLen = storage - numArgs * sizeof(LPSTR);
	LPSTR buffer = ((LPSTR)result) + numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++i)
	{
		if (bufLen <= 0) {
			*pNumArgs = 0;
			return NULL;
		}
		BOOL lpUsedDefaultChar = FALSE;
		retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, buffer, bufLen, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval))
		{
			LocalFree(result);
			LocalFree(args);
			return NULL;
		}

		result[i] = buffer;
		buffer += retval;
		bufLen -= retval;
	}

	LocalFree(args);

	*pNumArgs = numArgs;
	return result;
}
#endif

int main(int argc, char *argv[])
{
	json_t *GameCfg;
	int result;

	// Init globals
    errno = 0;
	memset(&CONFIG, 0, sizeof(CONFIG));
	result = Interface_Init(argc, argv);
	if(result != 0) {
		return result;
	}

	// Save current directory
	CONFIG.PROGDIR = malloc(MAX_PATH);
	if (!CONFIG.PROGDIR) {
		CURRERROR = errCRIT_MALLOC;
		ErrCracker(CURRERROR);
		return -1;
	}
	getcwd(CONFIG.PROGDIR, MAX_PATH);

	// Load profile if needed
	if (!CONFIG.CURRPROF) {
		result = Interface_EditConfig();
		if (CURRERROR == errUSR_ABORT || strndef(CONFIG.CURRDIR)) {
			// User wants to quit
			return result;
		}
	}

	// Load program preferences/locations
	GameCfg = JSON_Load(CONFIG.GAMECONFIG);
	if (!GameCfg) {
		ErrCracker(CURRERROR);
		return -1;
	}

	// Load SQL stuff
	if (!SQL_Load()) {
		ErrCracker(CURRERROR);
		return -1;
	}

	if (!SQL_Populate(GameCfg)) {
		ErrCracker(CURRERROR);
		sqlite3_close(CURRDB);
		return -1;
	}

	json_decref(GameCfg);

	// Display main dialog/inteface
	result = Interface_MainLoop();

	// Save checksum if exiting cleanly
	if (CURRERROR == errNOERR) {
		char *FilePath = NULL;
		FilePath = File_GetPath(1);
		if (strndef(FilePath)) { return -1; }

		CONFIG.CHECKSUM = crc32File(FilePath);
		Profile_DumpLocal(&CONFIG);

		safe_free(FilePath);
	}

	sqlite3_close(CURRDB);
	return result;
}

#ifdef HAVE_WINDOWS_H
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  WinMain
 *  Description:  Application entry point on Win32.
 *                Inits globals, loads profile, loads database, and runs the main loop.
 *                On a clean exit it records the game's checksum in the profile.
 *                This way if it's tampered by another program, the mod loader can
 *                detect this.
 *
 *                In the future WinMain should be a thunk to a more generic main().
 * =====================================================================================
 */
int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInst, char * cmdParam, int cmdShow)
{
	int argc;
	LPCWSTR argv;

	CURRINSTANCE = hInst;
	argv = CommandLineToArgvA(GetCommandLine(), &argc);

	main(argc, argv);
}
#endif
