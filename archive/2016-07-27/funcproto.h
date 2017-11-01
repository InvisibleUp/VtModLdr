#include <windows.h>
#include <stdint.h>
#include <sqlite3.h>
#include <jansson.h>
#include <stdint.h>

#ifndef FUNCPROTO_H_ENUMS
#define FUNCPROTO_H_ENUMS
//Error codes!
enum errCode {
	///Non-errors
	errNOERR,    	//No error.
	errUSR_ABORT,	//User initiated abort, rollback and get to main menu
	errUSR_QUIT, 	//User exited program, rollback and exit program
	
	///Critical errors
	errCRIT_DBASE,  	//Internal database error.
	errCRIT_FILESYS,	//Internal filesystem error.
	errCRIT_FUNCT,  	//Function call that should not fail just failed (Super-generic)
	errCRIT_ARGMNT, 	//Invalid function argument
	errCRIT_MALLOC, 	//Malloc failed. Panic.
	
	///Requires user intervention
	errWNG_NOSPC,   	//No space left in file for patches.
	errWNG_BADDIR,  	//Directory does not exist or contain expected contents
	errWNG_BADFILE, 	//Files does not exist or contain expected contents
	errWNG_CONFIG,  	//Invalid/missing configuration file
	errWNG_READONLY,	//File or folder is read-only
	errWNG_MODCFG   	//Invalid or missing parameter in mod metadata
};
//#endif

//#ifdef WinMain
//Structs and varibles (only declare once)
struct ModSpace {
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
	BOOL persist;
};

struct ProgConfig {
	char *CURRDIR;   	//Current root directory of game
	char *PROGDIR;   	//Directory program started in
	char *CURRPROF;  	//Path to currently loaded profile
	char *GAMECONFIG;	//Path to currently loaded game config file
	char *RUNPATH;   	//Command to execute when "run" button is pressed.
	int CHECKSUM;           //Checksum of loaded main file
};

extern enum errCode CURRERROR;

//Function return enums (oboslete)
enum Mod_CheckConflict_RetVals {ERR, GO, CANCEL};

extern struct ProgConfig CONFIG;    //Global program configuration

#define PROGVERSION_MAJOR 1        // Major version. Compatibility-breaking.
#define PROGVERSION_MINOR 0        // Minor version. Shiny new features.
#define PROGVERSION_BUGFIX 0       // Bugfix version. You'll need this. :)
extern const long PROGVERSION;           // Program version in format 0x00MMmmbb
//Name of program's primary author and website
extern const char PROGAUTHOR[];
extern const char PROGSITE[];

extern sqlite3 *CURRDB;                   //Current database holding patches

#endif

//Random helper functions
int ItoaLen(int input);
int FtoaLen(double input);
double MultiMax(double count, ...);
void ErrNo2ErrCode(void);

//Win32 helper functions
int GetUsedSpaceBytes(const char *ModUUID, int File);

//SQLite helper functions
const char * SQL_ColName(sqlite3_stmt * stmt, int col);
const char * SQL_ColText(sqlite3_stmt * stmt, int col);
int SQL_GetNum(sqlite3_stmt *stmt);
json_t * SQL_GetJSON(sqlite3_stmt *stmt);
int SQL_HandleErrors(int lineno, int SQLResult);

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
char *Mod_GetUUIDFromWin(HWND hwnd);

//Profile functions
void Profile_EmptyStruct(struct ProgConfig *LocalConfig);
char * Profile_GetGameName(const char *cfgpath);
char * Profile_GetGameVer(const char *cfgpath);
char *Profile_GetGameEXE(const char *cfgpath);
struct ProgConfig * Profile_Load(char *fpath);
void Profile_DumpLocal(struct ProgConfig *LocalConfig);
char * Profile_FindMetadata(const char *gamePath);
void Profile_Save(
	const char *profile,
	const char *game,
	const char *gameConf,
	int checksum,
	const char *runpath
);

//Variable functions
enum VarType Var_GetType(const char *type);
const char * Var_GetType_Str(enum VarType type);
struct VarValue Var_GetValue_SQL(const char *VarUUID);
struct VarValue Var_GetValue_JSON(json_t *VarObj, const char *ModUUID);
BOOL Var_ClearEntry(const char *ModUUID);
BOOL Var_UpdateEntry(struct VarValue result);
BOOL Var_MakeEntry(struct VarValue result);
BOOL Var_MakeEntry_JSON(json_t *VarObj, const char *ModUUID);

//For Win32
extern HINSTANCE CURRINSTANCE;            //Current instance of application
