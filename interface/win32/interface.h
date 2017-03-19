//Win32 GUI
//Public functions (expected to be implemented by other interface modes)
//TODO: Make platform-agnostic
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

//Info I/O
void AlertMsg (const char *Message, const char *Title);
BOOL PromptMsg(const char *Message, const char *Title);

typedef HWND ProgDialog_Handle;
ProgDialog_Handle ProgDialog_Init(int max, const char *label);
void ProgDialog_Update(ProgDialog_Handle ProgDialog, int Delta);
void ProgDialog_Kill(ProgDialog_Handle ProgDialog);

//Folder/file selection
char * SelectFolder(LPCSTR title);
char * SelectFile(HWND hwnd, int *nameOffset, const char *Filter);
char * SaveFile(HWND hwnd, const char *Filter, const char *DefExt);

//Variable things
//void Var_CreatePubList(json_t *PubList, const char *UUID);
//HWND Var_CreateListBox(HWND hParent, const char *VarUUID);

/* Things we probably should add */
int Interface_Init(int argc, char *argv[]);			// Set classes/outputs/etc. for interface
int Interface_EditConfig(void);	// Bring up profile editor
int Interface_MainLoop(void);   	// Bring up main dialog with mods
int Interface_RunProgram(void);	// Run program in profile
// ViewModInfo();       // View mod name/desc (do we need?)
// EditModVars();   	// Bring up variable dialog for mod
// ViewCopyInfo();  	// Bring up About dialog (or equiv)
// ViewModPatches();	// For future use

#ifdef __cplusplus
}
#endif