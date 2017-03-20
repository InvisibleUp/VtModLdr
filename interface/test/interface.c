// Stub interface for unit testing

#include "../../includes.h"
#include "interface.h"
#include <stdio.h>

char *TESTNAME = NULL;

// This file is designed to bridge any given interface with the rest of the code.
// This keeps the "buisness logic" seperate from the GUI in the event I want to
// port this to another system (or to, say, QT.)

//Info I/O
void AlertMsg(const char *Message, const char *Title) {
	printf("\n%s\n====================\n%s\n\n", Title, Message);
}
BOOL PromptMsg(const char *Message, const char *Title) {
	printf("\n[PROMPT] %s\n====================\n%s\n\n", Title, Message);
	return TRUE;
}

ProgDialog_Handle ProgDialog_Init(int max, const char *label) {
	ProgDialog_Handle retval = calloc(1, sizeof(struct ProgStruct));
    retval->max = max;
    retval->val = 0;
    retval->msg = strdup(label);
    return retval;
}
void ProgDialog_Update(ProgDialog_Handle ProgDialog, int Delta) {
	if (!ProgDialog) { return; }
	ProgDialog->val += Delta;
 
    // Calculuate the ratio of complete-to-incomplete.
    const int w = 70;
    const double ratio = ProgDialog->val/(double)ProgDialog->max;
    const int drawCount = ratio * w;
 
    // Show the percentage complete.
    //printf("%s: %3d%% [", ProgDialog->msg, (int)(ratio*100) );
    // Show the load bar.
    for (int i = 0; i<drawCount; i++){
    //   putchar('=');
    }
    for (int i = drawCount; i<w; i++){
    //   putchar(' ');
    }
   // putchar(']');
    
    // ANSI Control codes to go back to the
    // previous line and clear it.
    //fflush(stdout);
    //printf("\r");
}
void ProgDialog_Kill(ProgDialog_Handle ProgDialog) {
    safe_free(ProgDialog->msg);
	safe_free(ProgDialog);
   // putchar('\n');
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Interface_Init
 *  Description:  Registers window classes, gets default font, etc.
 *                Essentially initializes the interface.
 *                [Implemented as stub]
 * =====================================================================================
 */
int Interface_Init(int argc, char *argv[])
{
	if (argc < 2) {
		//TESTNAME = "ALL";
		puts("[FAIL] Test not specified");
		return 1;
	} else {
		TESTNAME = argv[1]; 
	}

	// Win32 output redirection
#ifdef HAVE_WINDOWS_H
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
#endif
    /*{
        char *FilePath = NULL;
        char WorkingDir[256];
        
        getcwd(WorkingDir, 255);
        
        asprintf(&FilePath, "%s/mods.db", WorkingDir);
        File_Delete(FilePath); // Refresh mod db
        safe_free(FilePath);
    }*/
    
    File_Delete("mods.db");
    
    ErrNo2ErrCode();
    if(CURRERROR == errWNG_BADFILE){
        // Can't delete something that doesn't exist
        CURRERROR = errNOERR;
    }
    
	return 0;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Interface_MainLoop
 *  Description:  Main command loop that handles user input and preforms actions
 *                accordingly. [Thunk to autogenerated tests/Test_Caller.c]
 * =====================================================================================
 */
int Interface_MainLoop(void)
{
	//fprintf(stderr, "Testing %s!\n\n", TESTNAME);
	return Test_Caller(TESTNAME);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Interface_EditConfig
 *  Description:  Opens a dialog or equivelent that allows the user to edit the
 *                configuration. This is called on program start tp verify a pre-loaded
 *                profile or to generate one if it is missing.
 *                [Implemented as stub]
 * =====================================================================================
 */
int Interface_EditConfig(void)
{
	struct ProgConfig *LocalConfig = malloc(sizeof(struct ProgConfig));
	const char *fpath = "test_profile.json";
    char *TestBin = NULL;
	memset(LocalConfig, 0, sizeof(struct ProgConfig));

	// Record paths
	LocalConfig->PROGDIR = strdup(CONFIG.PROGDIR);
	LocalConfig->CURRPROF = strdup(fpath);
	LocalConfig->CURRDIR = malloc(255);
	getcwd(LocalConfig->CURRDIR, 255);
    
    // Get test.bin path
    asprintf(&TestBin, "%s/test.bin", LocalConfig->CURRDIR);

	// Create dummy "game", 256 KB in size
	if (
        !File_Exists(TestBin, FALSE, FALSE) || 
        !Proto_Checksum("_InitSeq", "test.bin", 0xE20EEA22)
    ) {
        CURRERROR = errNOERR;
		File_Delete(TestBin);
        
		if (!File_Create(TestBin, 256 * 1024)) {
			puts("Could not create test.bin!");
			ErrCracker(CURRERROR);
            safe_free(TestBin);
			return 1;
		}
	}
	safe_free(TestBin);

	// Set dummy game info
	asprintf(&LocalConfig->GAMECONFIG, "%s/games/test.json", LocalConfig->PROGDIR);
	LocalConfig->GAMEUUID = strdup("testuuid");
	LocalConfig->GAMEVER = strdup("testver");
	LocalConfig->RUNPATH = strdup("test.bin");
	LocalConfig->CHECKSUM = 0;

	// Copy all of this to CONFIG
	Profile_EmptyStruct(&CONFIG);
	memcpy(&CONFIG, LocalConfig, sizeof(struct ProgConfig));
    safe_free(LocalConfig);

	return 0;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Interface_RunProgram
*  Description:  BLOCKING function that runs the program specified in the user's
*                profile. [Implemented as stub.]
* =====================================================================================
*/
int Interface_RunProgram()
{
	return 0;
}
