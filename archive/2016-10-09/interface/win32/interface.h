//Public functions (expected to be implemented by other interface modes)
//TODO: Make platform-agnostic

//Info I/O
void AlertMsg (const char *Message, const char *Title);
HWND ProgDialog_Init(int max, const char *label);
void ProgDialog_Update(HWND ProgDialog, int Delta);
void ProgDialog_Kill(HWND ProgDialog);

//Folder/file selection
char * SelectFolder(LPCSTR title);
char * SelectFile(HWND hwnd, int *nameOffset, const char *Filter);
char * SaveFile(HWND hwnd, const char *Filter, const char *DefExt);

//Variable things
void Var_CreatePubList(json_t *PubList, const char *UUID);
//HWND Var_CreateListBox(HWND hParent, const char *VarUUID);

/* Things we probably should add */
void InitInterface(void);     // Set classes/outputs/etc. for interface
void EditConfig(void);    	// Bring up profile editor
int ViewModList(void);   	// Bring up main dialog with mods
// ViewModInfo();       // View mod name/desc (do we need?)
// EditModVars();   	// Bring up variable dialog for mod
// ViewCopyInfo();  	// Bring up About dialog (or equiv)
// ViewModPatches();	// For future use