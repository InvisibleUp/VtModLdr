#pragma once

//Win32 helper functions
void AlertMsg (char *Message, char *Title);
void EnableWholeWindow(HWND hwnd, BOOL state);
const char * SelectFolder(LPCSTR title);
void SelectFile(HWND hwnd, char *fpath, char *fname, char *Filter);
void UpdateModList(HWND list);
void UpdateModDetails(HWND hwnd, int listID);
long GetUsedSpaceNum(char *ModUUID, int File);

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
const char * JSON_GetStr(json_t *root, char *name);
int JSON_GetStrLen(json_t *root, char *name);

//Generic I/O helper functions
BOOL File_Exists(char file[MAX_PATH], BOOL InFolder, BOOL ReadOnly);
BOOL File_Known(char FileName[MAX_PATH], char *GameName, int handle, json_t *whitelist);
int File_OpenSafe(char filename[MAX_PATH], int flags);
void File_WriteBytes(int filehandle, int offset, unsigned char *data, int datalen);
unsigned char * Hex2Bytes(const char *hexstring, int len);

//UI-specific IO functions
const char * SelectInstallFolder(json_t *GameCfg);

//Mod loading functions
BOOL Mod_CheckCompat(json_t *root);
BOOL Mod_CheckConflict(json_t *root);
BOOL Mod_CheckDep(json_t *root);
BOOL Mod_FindDep(char *ModUUID);

//Mod installation functions
unsigned char * Mod_GetBytes(json_t *patchCurr, int *len);
int GetFileID(char * FileName);
char * GetFileName(int input);
int MakeFileEntry(char *FileName);
struct ModSpace Mod_FindSpace(struct ModSpace input, int len, char *ModUUID, BOOL Clear);
BOOL ModOp_Clear(struct ModSpace input, char *ModUUID);
int Mod_AddSpace(struct ModSpace input, int start, int len, char *ModUUID);
BOOL ModOp_Add(struct ModSpace input, int handle, unsigned char * bytes, int len, char *ModUUID, BOOL Mod_AddSpace);
BOOL Mod_Install(json_t *root);

//Mod removal functions
void ModOp_UnAdd(json_t *row);
void ModOp_UnClear(json_t *row);
BOOL Mod_Uninstall(char *ModUUID);

//Win32 GUI components
BOOL CALLBACK Dlg_Patch(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK Dlg_About(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK Dlg_Main(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInst, char * cmdParam, int cmdShow);


// ValidEXE struct (should move to a main.h)
struct ValidEXE{                       // Structure to define supported EXE files
	long size;                      // Size of file
	unsigned long crc32;           // CRC32 Checksum of file
	char mod1[64];                 // ID of pre-installed mod 1 (think CPU-fix)
	char mod2[64];                 // ID of pre-installed mod 2
};

struct FileID{
	int ID;
	char path[MAX_PATH];
};

struct ModSpace{
	char ID[256];
	int Start;
	int End;
	int FileID;
};