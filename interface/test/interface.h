#pragma once
// Public functions (expected to be implemented by other interface modes)
// Testing stub interface

//#include <windows.h> // grumble grumble...
#include "../../tests/tests.h" // Function definitions for tests

#ifdef __cplusplus
extern "C" {
#endif
    
struct ProgStruct {
    int max;
    int val;
    char *msg;
};

//Info I/O
void AlertMsg (const char *Message, const char *Title);
BOOL PromptMsg(const char *Message, const char *Title);

typedef struct ProgStruct * ProgDialog_Handle;
ProgDialog_Handle ProgDialog_Init(int max, const char *label);
void ProgDialog_Update(ProgDialog_Handle ProgDialog, int Delta);
void ProgDialog_Kill(ProgDialog_Handle ProgDialog);

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

/* Generic testing prototypes */
int Test_Caller(const char *testname);
int Proto_Checksum(
	const char *filename, unsigned int expected, BOOL DispError
);
int Proto_DBase_OK(void);
json_t * Proto_DBase_ToJSON(const char *query);
int Proto_JSON_CheckOutput(
	json_t *out,
	const char *keyname,
	const char *values[],
	const unsigned int vallen
);

#ifdef __cplusplus
}
#endif
