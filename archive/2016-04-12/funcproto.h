#pragma once
#include <stdint.h>
//Function return enums
enum Mod_CheckConflict_RetVals {ERR, GO, CANCEL};

//CRC32 routine
unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned long len);
unsigned long crc32File(const char *filename);

//asprintf
int vasprintf(char **str, const char *fmt, va_list ap);
int asprintf(char **str, const char *fmt, ...);

//Random helper functions
int ItoaLen(int input);
int FtoaLen(double input);
double MultiMax(double count, ...);
void ErrNo2ErrCode(void);

//Win32 helper functions
void AlertMsg (const char *Message, const char *Title);
void EnableWholeWindow(HWND hwnd, BOOL state);
char * SelectFolder(LPCSTR title);
char * SelectFile(HWND hwnd, int *nameOffset, const char *Filter);
BOOL UpdateModList(HWND list);
BOOL UpdateModDetails(HWND hwnd, int listID);
int GetUsedSpaceBytes(const char *ModUUID, int File);
BOOL CALLBACK SetFont(HWND child, LPARAM font);
void FixDlgFont(DLGTEMPLATE *pTemplate, unsigned short size);
HWND CreateDialogSysFont(int DlgIDD, DLGPROC DlgProc, HWND hParent);
void DialogBoxSysFont(int DlgIDD, DLGPROC DlgProc, HWND hParent);

//SQLite helper functions
const char * SQL_ColName(sqlite3_stmt * stmt, int col);
const char * SQL_ColText(sqlite3_stmt * stmt, int col);
int SQL_GetNum(sqlite3_stmt *stmt);
json_t * SQL_GetJSON(sqlite3_stmt *stmt);
int SQL_HandleErrors(int SQLResult);

BOOL SQL_Load(void);
BOOL SQL_Populate(json_t *GameCfg);

//Jansson helper functions
json_t * JSON_Load(const char *fpath);
signed long JSON_GetInt(json_t *root, const char *name);
char * JSON_GetStr(json_t *root, const char *name);
int JSON_GetStrLen(json_t *root, const char *name);

//Generic I/O helper functions
BOOL File_Exists(const char *file, BOOL InFolder, BOOL ReadOnly);
BOOL File_Known(const char *FileName, json_t *whitelist);
int File_OpenSafe(const char *filename, int flags);
void File_WriteBytes(int filehandle, int offset, unsigned const char *data, int datalen);
unsigned char * Hex2Bytes(const char *hexstring, int *len);
char * Bytes2Hex(unsigned const char *bytes, int len);
char * SelectInstallFolder(json_t *GameCfg);
#ifndef filesize
long filesize(const char *filename);
#endif

//Mod loading functions
BOOL Mod_CheckCompat(json_t *root);
enum Mod_CheckConflict_RetVals Mod_CheckConflict(json_t *root);
BOOL Mod_CheckDep(json_t *root);
void Mod_CheckDepAlert(json_t *root);
BOOL Mod_FindDep(const char *ModUUID);
BOOL Mod_FindUUIDLoc(int *start, int *end, const char *UUID);
unsigned char * Mod_GetBytes(json_t *patchCurr, int *len, int fhandle);

//Mod installation functions
int File_GetID(const char * FileName);
char * File_GetName(int input);
int File_MakeEntry(const char *FileName);

struct ModSpace Mod_GetPatchInfo(json_t *patchCurr, int fhandle);
struct ModSpace Mod_FindSpace(struct ModSpace input, const char *ModUUID, BOOL Clear);
BOOL ModOp_Clear(struct ModSpace input, const char *ModUUID);
BOOL Mod_AddSpace(struct ModSpace input, int start, const char *ModUUID);
BOOL ModOp_Add(struct ModSpace input, int handle, const char *ModUUID);
BOOL Mod_Install(json_t *root);
BOOL Mod_AddToDB(json_t *root);

//Mod removal functions
BOOL ModOp_UnAdd(json_t *row);
BOOL ModOp_UnClear(json_t *row);
BOOL Mod_Uninstall(const char *ModUUID);
const char *Mod_GetUUIDFromWin(HWND hwnd);

//Win32 GUI components
BOOL CALLBACK Dlg_Patch(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK Dlg_Main(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK Dlg_Generic(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

//Structs
struct ModSpace{
	char *ID;
	int FileID;
	int Start;
	int End;

	int SrcStart;
	int SrcEnd;

	unsigned char *Bytes;
	int Len;

	BOOL Valid; //False if invalid due to errors, etc.
};
#define MODSPACE_ID_MAX 255

enum VarType {INVALID, Int32, uInt32, Int16, uInt16, Int8, uInt8, IEEE32, IEEE64};
struct VarValue {
	enum VarType type;
	char *UUID;
	char *desc;
	char *publicType;
	char *mod;
	union {
		int32_t Int32;
		uint32_t uInt32;
		int16_t Int16;
		uint16_t uInt16;
		int8_t Int8;
		uint8_t uInt8;
		float IEEE32;
		double IEEE64;
	};
};

//Error codes!
enum errCode {
	///Non-errors
	errNOERR,	//No error.
	errUSR_ABORT,	//User initiated abort, rollback and get to main menu
	errUSR_QUIT,	//User exited program, rollback and exit program

	///Critical errors
	errCRIT_DBASE,	//Internal database error.
	errCRIT_FILESYS,	//Internal filesystem error.
	errCRIT_FUNCT,	//Function call that should not fail just failed (Super-generic)
	errCRIT_ARGMNT, //Invalid function argument
	errCRIT_MALLOC, //Malloc failed. Panic.

	///Requires user intervention
	errWNG_NOSPC,	//No space left in file for patches.
	errWNG_BADDIR,	//Directory does not exist or contain expected contents
	errWNG_BADFILE,	//Files does not exist or contain expected contents
	errWNG_CONFIG,	//Invalid/missing configuration file
	errWNG_READONLY,	//File or folder is read-only
	errWNG_MODCFG	//Invalid or missing parameter in mod metadata
};

// These are for access() in io.h. They're defined somewhere I'm sure
// but I couldn't tell you where.
#ifndef F_OK
#define F_OK 0	// Check for existence
#define X_OK 1	// Check for execute permission
#define W_OK 2	// Check for write permission
#define R_OK 4	// Check for read permission
#endif
