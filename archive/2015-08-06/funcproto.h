#pragma once
//CRC32 routine
unsigned long crc32(const int fd, unsigned int len);

//asprintf
vasprintf(char **str, const char *fmt, va_list ap);
int asprintf(char **str, const char *fmt, ...);

//Random helper functions
int ItoaLen(size_t input);
int FtoaLen(double input);
int MultiMax(unsigned int count, ...);

//Win32 helper functions
void AlertMsg (char *Message, char *Title);
void EnableWholeWindow(HWND hwnd, BOOL state);
const char * SelectFolder(LPCSTR title);
void SelectFile(HWND hwnd, char *fpath, char *fname, char *Filter);
void UpdateModList(HWND list);
void UpdateModDetails(HWND hwnd, int listID);
int GetUsedSpaceNum(char *ModUUID, int File);

//SQLite helper functions
char * SQL_ColName(sqlite3_stmt * stmt, int col);
char * SQL_ColText(sqlite3_stmt * stmt, int col);
int SQL_GetNum(char *query);
json_t * SQL_GetJSON(char *query);

void SQL_Load();
void SQL_Populate(json_t *GameCfg);

//Jansson helper functions
json_t * JSON_Load(char *fpath);
size_t JSON_GetInt(json_t *root, char *name);
char * JSON_GetStr(json_t *root, char *name);
int JSON_GetStrLen(json_t *root, char *name);

//Generic I/O helper functions
BOOL File_Exists(char file[MAX_PATH], BOOL InFolder, BOOL ReadOnly);
BOOL File_Known(char FileName[MAX_PATH], char *GameName, int handle, json_t *whitelist);
int File_OpenSafe(char filename[MAX_PATH], int flags);
void File_WriteBytes(int filehandle, int offset, unsigned char *data, int datalen);
unsigned char * Hex2Bytes(const char *hexstring, int len);
int SelectInstallFolder(json_t *GameCfg);

//Mod loading functions
BOOL Mod_CheckCompat(json_t *root);
BOOL Mod_CheckConflict(json_t *root);
BOOL Mod_CheckDep(json_t *root);
void Mod_CheckDepAlert(json_t *root);
BOOL Mod_FindDep(char *ModUUID);
unsigned char * Mod_GetBytes(json_t *patchCurr, int *len);

//Mod installation functions
int File_GetID(char * FileName);
char * File_GetName(int input);
int File_MakeEntry(char *FileName);

struct ModSpace Mod_FindSpace(struct ModSpace input, int len, char *ModUUID, BOOL Clear);
BOOL ModOp_Clear(struct ModSpace input, char *ModUUID);
void Mod_AddSpace(struct ModSpace input, int start, int len, char *ModUUID);
BOOL ModOp_Add(struct ModSpace input, int handle, unsigned char * bytes, int len, char *ModUUID, BOOL Mod_AddSpace);
BOOL Mod_Install(json_t *root);

//Mod removal functions
void ModOp_UnAdd(json_t *row);
void ModOp_UnClear(json_t *row);
BOOL Mod_Uninstall(char *ModUUID);

//Win32 GUI components
BOOL CALLBACK Dlg_Patch(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK Dlg_Main(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK Dlg_Generic(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInst, char * cmdParam, int cmdShow);


// ValidEXE struct (should move to a main.h)
struct ValidEXE{                       // Structure to define supported EXE files
	long size;                      // Size of file
	unsigned long crc32;           // CRC32 Checksum of file
	char mod1[64];                 // ID of pre-installed mod 1 (think CPU-fix)
	char mod2[64];                 // ID of pre-installed mod 2
};

/*struct FileID{
	int ID;
	char path[MAX_PATH];
};*/

struct ModSpace{
	char ID[256];
	int Start;
	int End;
	int FileID;
};

enum { INVALID, SINT, USINT, IEEE } varType;
struct VarValue{
	enum VarType type;
	union {
		INT32 sint;
		UINT32 usint;
		double ieee;
	};
};

// These are for access() in io.h. They're defined somewhere I'm sure
// but I couldn't tell you where.
#ifndef F_OK
#define  F_OK 0 	               // Check for existence
#define  X_OK 1 	               // Check for execute permission
#define  W_OK 2 	               // Check for write permission
#define  R_OK 4 	               // Check for read permission
#endif		
