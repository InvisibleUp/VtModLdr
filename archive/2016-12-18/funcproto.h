#include <stdint.h>
#include <sqlite3.h>
#include <jansson.h>

// Can't use <windows.h> from here in C++ mode
// Thankfully this is the only typedef needed
#ifndef BOOL
	typedef int                 BOOL;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FUNCPROTO_H_ENUMS
#define FUNCPROTO_H_ENUMS
//Error codes!
enum errCode {
	///Non-errors
	errNOERR,       	//No error.
	errUSR_ABORT,   	//User initiated abort, rollback and get to main menu
	errUSR_QUIT,    	//User exited program, rollback and exit program
	errUSR_CONFIRM, 	//User wishes for action to occur. Use when NOERR is ambiguous.
	
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
	errWNG_MODCFG,   	//Invalid or missing parameter in mod metadata
	errWNG_EXISTS           //File/dir/mod already exists
};
//#endif

//#ifdef WinMain
//Structs and variables (only declare once)
struct ModSpace {
	char *ID;
	long FileID;
	long Start;
	long End;
	
	//No SrcFileID (source file is usually in mod installer)
	long SrcStart;
	long SrcEnd;
	
	unsigned char *Bytes;
	long Len;
	
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
		unsigned char raw[8];
	};
	BOOL persist;
};

struct ProgConfig {
	char *CURRDIR;   	//Current root directory of game
	char *PROGDIR;   	//Directory program started in
	char *CURRPROF;  	//Path to currently loaded profile
	char *GAMECONFIG;	//Path to currently loaded game config file
	char *RUNPATH;   	//Command to execute when "run" button is pressed.
	unsigned long CHECKSUM;		//Checksum of loaded main file,
	char *GAMEVER;		//ID for game version selected
	char *GAMEUUID;         //UUID for the game selected
};

extern enum errCode CURRERROR;

//Function return enums (obsolete)
//enum Mod_CheckConflict_RetVals {ERR, GO, CANCEL};

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
void ErrNo2ErrCode(void);
void ErrCracker(enum errCode error);
char * ForceStrNumeric(const char *input);

//Interface helper functions
int GetUsedSpaceBytes(const char *ModUUID, int File);

//SQLite helper functions
const char * SQL_ColName(sqlite3_stmt * stmt, int col);
const char * SQL_ColText(sqlite3_stmt * stmt, int col);
int SQL_GetNum(sqlite3_stmt *stmt);
json_t * SQL_GetJSON(sqlite3_stmt *stmt);
char * SQL_GetStr(sqlite3_stmt *stmt);
int SQL_HandleErrors(const char *filename, int lineno, int SQLResult);

BOOL SQL_Load(void);
BOOL SQL_Populate(json_t *GameCfg);

//Jansson helper functions
json_t * JSON_Load(const char *fpath);
unsigned long JSON_GetuInt(json_t *root, const char *name);
signed long JSON_GetInt(json_t *root, const char *name);
double JSON_GetDouble(json_t *root, const char *name);
char * JSON_GetStr(json_t *root, const char *name);
int JSON_GetStrLen(json_t *root, const char *name);

//Generic I/O helper functions
BOOL File_Exists(const char *file, BOOL InFolder, BOOL ReadOnly);
BOOL File_Known(const char *FileName, json_t *whitelist);
int File_OpenSafe(const char *filename, int flags);
void File_WriteBytes(
	int filehandle, 
	int offset,
	unsigned const char *data,
	int datalen
);
void File_WritePattern(
	int filehandle,
	int offset,
	unsigned const char *data,
	int datalen,
	int blocklen
);
unsigned char * Hex2Bytes(const char *hexstring, int *len);
char * Bytes2Hex(unsigned const char *bytes, int len);

void File_Copy(const char *OldPath, const char *NewPath);
void File_Delete(const char *Path);
BOOL File_MovTree(char *srcPath, char *dstPath);
BOOL File_DelTree(char *DirPath);
BOOL File_Create(char *FilePath, int FileLen);
#ifndef filesize
long filesize(const char *filename);
#endif

//Mod loading functions
BOOL Mod_CheckCompat(json_t *root);
BOOL Mod_CheckConflict(json_t *root);
BOOL Mod_CheckDep(json_t *root);
void Mod_CheckDepAlert(json_t *root);
BOOL Mod_FindDep(const char *ModUUID);
void Mod_FindDepAlert(const char *ModUUID);
BOOL Mod_FindUUIDLoc(int *start, int *end, const char *UUID);
BOOL Mod_PatchFillUUID(
	struct ModSpace *input, BOOL IsSrc,
	const char *FileType, const char *FileName,
	const char *FilePath, const char *UUID
);
BOOL Mod_PatchKeyExists(json_t *patchCurr, const char *KeyName, BOOL ShowAlert);
BOOL Mod_Verify(json_t *root);

//Mod installation functions
int File_GetID(const char *FileName);
char * File_GetName(int input);
int File_MakeEntry(const char *FileName);
char * File_FindPatchOwner(const char *PatchUUID);

struct ModSpace Mod_GetPatchInfo(json_t *patchCurr, const char *ModPath);
struct ModSpace Mod_FindSpace(const struct ModSpace *input, BOOL IsClear);
struct ModSpace Mod_FindParentSpace(const struct ModSpace *input);
struct ModSpace Mod_GetSpace(const char *PatchUUID);
char * Mod_GetSpaceType(const char *PatchUUID);
BOOL Mod_SpaceExists(const char *PatchUUID);

BOOL Mod_MakeSpace(struct ModSpace *input, const char *ModUUID, BOOL IsClear);
BOOL Mod_RenameSpace(const char *OldID, const char *NewID);
/*BOOL Mod_MergeSpace(
	const struct ModSpace *input,
	const char *HeadID,
	const char *TailID,
	const char *ModUUID
);*/
BOOL Mod_SplitSpace(
	struct ModSpace *out,
	const char *HeadID,
	const char *TailID,
	const char *OldID,
	const char *ModUUID,
	int splitOff,
	BOOL retHead
);
BOOL Mod_SpliceSpace(
	struct ModSpace *parent,
	struct ModSpace *child,
	const char *ModUUID
);
BOOL Mod_CreateRevertEntry(const struct ModSpace *input);

BOOL ModOp_Clear(struct ModSpace *input, const char *ModUUID);
BOOL ModOp_Add(struct ModSpace *input, const char *ModUUID);
BOOL ModOp_Reserve(struct ModSpace *input, const char *ModUUID);

BOOL Mod_InstallSeries(const char *ModList);
BOOL Mod_Install(json_t *root, const char *path);
BOOL Mod_AddToDB(json_t *root, const char *path);

BOOL Mod_ClaimSpace(const char *PatchUUID, const char *ModUUID);
BOOL Mod_UnClaimSpace(const char *PatchUUID);

//Mod uninstallation functions
char * Mod_UninstallSeries(const char *UUID);
BOOL Mod_Uninstall(const char *ModUUID);
BOOL Mod_Reinstall(const char *ModUUID);
BOOL Mod_Uninstall_Remove(const char *PatchUUID);
//BOOL Mod_Uninstall_Restore(const char *PatchUUID);
BOOL ModOp_UnMerge(json_t *row);
BOOL ModOp_UnSplit(json_t *row);
BOOL ModOp_UnSpace(json_t *row);

int Mod_GetVerCount(const char *PatchUUID);
char * Mod_MakeBranchName(const char *PatchUUID);
BOOL Mod_RemoveAllClear();

//PE support functions
BOOL File_IsPE(const char *FilePath);
int File_PEToOff(const char *FilePath, uint32_t PELoc);
int File_OffToPE(const char *FilePath, uint32_t FileLoc);

//Mod whole file functions
char *Mod_MangleUUID(const char *UUID);

//Mod removal functions
BOOL ModOp_UnAdd(json_t *row);
BOOL ModOp_UnClear(json_t *row);
BOOL Mod_Uninstall(const char *ModUUID);

//Profile functions
void Profile_EmptyStruct(struct ProgConfig *LocalConfig);
char * Profile_GetGameName(const char *cfgpath);
char * Profile_GetGameVer(const char *cfgpath);
char * Profile_GetGameVerID(const char *cfgpath);
char * Profile_GetGameEXE(const char *cfgpath);
char * Profile_GetGameUUID(const char *cfgpath);
struct ProgConfig * Profile_Load(char *fpath);
void Profile_DumpLocal(struct ProgConfig *LocalConfig);
char * Profile_FindMetadata(const char *gamePath);
void Profile_Save(
	const char *profile,
	const char *game,
	const char *gameConf,
	int checksum,
	const char *runpath,
	const char *gamever
);

//Variable functions
enum VarType Var_GetType(const char *type);
const char * Var_GetType_Str(enum VarType type);
enum VarType Var_GetType_SQL(const char *VarUUID);

int Var_GetLen(const struct VarValue *var);
struct VarValue Var_GetValue_SQL(const char *VarUUID);
struct VarValue Var_GetValue_JSON(json_t *VarObj, const char *ModUUID);

BOOL Var_ClearEntry(const char *ModUUID);
BOOL Var_UpdateEntry(struct VarValue result);
BOOL Var_MakeEntry(struct VarValue result);
BOOL Var_MakeEntry_JSON(json_t *VarObj, const char *ModUUID);
BOOL Var_Compare(
	const struct VarValue *var, 
	const char *mode, 
	const void *input,
	const int inlen
);
void Var_CreatePubList(json_t *PubList, const char *UUID);
BOOL Var_WriteFile(struct VarValue input);
BOOL Var_Exists(const char *ID);

int Var_GetInt(const struct VarValue *var);
double Var_GetDouble(const struct VarValue *var);
void Var_Destructor(struct VarValue *var);

int Eq_Parse_Int(const char * eq, const char *ModPath);
unsigned int Eq_Parse_uInt(const char * eq, const char *ModPath);
double Eq_Parse_Double(const char * eq, const char *ModPath);

#ifdef __cplusplus
}
#endif