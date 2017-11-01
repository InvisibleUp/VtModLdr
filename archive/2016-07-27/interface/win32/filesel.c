#include <windows.h>
#include "../../includes.h"
#include "win32helper.h"
#include "interface.h"

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SelectFolder
 *  Description:  Wrapper around SHBrowseForFolder that is designed to
 *                get only valid folder names (not My Computer, for instance).
 * =====================================================================================
 */
char * SelectFolder(LPCSTR title)
{
	BROWSEINFO bi = {0}; // For init directory selector
	LPITEMIDLIST pidl;
	char *path = malloc(MAX_PATH);
	CURRERROR = errNOERR;
	bi.lpszTitle = title;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
		
	if(path == NULL){
		AlertMsg("Malloc Fail #3", "");
		CURRERROR = errCRIT_MALLOC;
		return strdup("");
	}
	strcpy(path, "");
	
	pidl = SHBrowseForFolder ( &bi );
	// If user hits Cancel, stop nagging user
	if (pidl == 0){
		CURRERROR = errUSR_ABORT;
		safe_free(path);
		return strdup("");
	}
		
	// Get the name of the folder
	if (!SHGetPathFromIDList ( pidl, path )){
		//User chose invalid folder
		CURRERROR = errWNG_BADDIR;
		safe_free(path);
		return strdup("");
	}
	return path;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SelectFile
 *  Description:  Simple wrapper around GetOpenFileName.
 * =====================================================================================
 */
char * SelectFile(HWND hwnd, int *nameOffset, const char *Filter)
{
	OPENFILENAME ofn;
	char *fpath = calloc(MAX_PATH, sizeof(char));
	memset(&ofn, 0, sizeof(OPENFILENAME));
	
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = fpath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
        ofn.lpstrFilter = Filter;
        ofn.lpstrCustomFilter = NULL;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.lpstrInitialDir = NULL;
        ofn.lpstrTitle = NULL;
	
	if(GetOpenFileName(&ofn) == FALSE){
		//Set CURRERROR
		
		DWORD DlgError = CommDlgExtendedError();
		switch(DlgError){
		case CDERR_DIALOGFAILURE:
			AlertMsg("CDERR_DIALOGFAILURE", "SelectFile error!");
		break;
		
		case CDERR_FINDRESFAILURE:
			AlertMsg("CDERR_FINDRESFAILURE", "SelectFile error!");
		break;
		
		case CDERR_INITIALIZATION:
			AlertMsg("CDERR_INITIALIZATION", "SelectFile error!");
		break;
		
		case CDERR_LOADRESFAILURE:
			AlertMsg("CDERR_LOADRESFAILURE", "SelectFile error!");
		break;
		
		case CDERR_LOADSTRFAILURE:
			AlertMsg("CDERR_LOADSTRFAILURE", "SelectFile error!");
		break;
		
		case CDERR_LOCKRESFAILURE:
			AlertMsg("CDERR_LOCKRESFAILURE", "SelectFile error!");
		break;
		
		case CDERR_MEMALLOCFAILURE:
			AlertMsg("CDERR_MEMALLOCFAILURE", "SelectFile error!");
		break;
		
		case CDERR_MEMLOCKFAILURE:
			AlertMsg("CDERR_MEMLOCKFAILURE", "SelectFile error!");
		break;
		
		case CDERR_NOHINSTANCE:
			AlertMsg("CDERR_NOHINSTANCE", "SelectFile error!");
		break;
		
		case CDERR_NOHOOK:
			AlertMsg("CDERR_NOHOOK", "SelectFile error!");
		break;
		
		case CDERR_NOTEMPLATE:
			AlertMsg("CDERR_NOTEMPLATE", "SelectFile error!");
		break;
		
		case CDERR_REGISTERMSGFAIL:
			AlertMsg("CDERR_REGISTERMSGFAIL", "SelectFile error!");
		break;
		
		case CDERR_STRUCTSIZE:
			AlertMsg("CDERR_STRUCTSIZE", "SelectFile error!");
		break;
		
		case FNERR_BUFFERTOOSMALL:
			AlertMsg("FNERR_BUFFERTOOSMALL", "SelectFile error!");
		break;
		
		case FNERR_INVALIDFILENAME:
			AlertMsg("FNERR_INVALIDFILENAME", "SelectFile error!");
		break;
		
		case FNERR_SUBCLASSFAILURE:
			AlertMsg("FNERR_SUBCLASSFAILURE", "SelectFile error!");
		break;
		}
	}
	//TODO: Error checking?

	//Get just the filename
	if(nameOffset != NULL){
		*nameOffset = ofn.nFileOffset;
	}
	
	return fpath;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SaveFile
 *  Description:  Simple wrapper around GetSaveFileName.
 * =====================================================================================
 */
char * SaveFile(HWND hwnd, const char *Filter, const char *DefExt)
{
	OPENFILENAME ofn;
	char *fpath = calloc(MAX_PATH, sizeof(char));
	memset(&ofn, 0, sizeof(OPENFILENAME));
	
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = fpath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
        ofn.lpstrFilter = Filter;
        ofn.lpstrCustomFilter = NULL;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.lpstrInitialDir = NULL;
        ofn.lpstrTitle = NULL;
	
	GetSaveFileName(&ofn);
	//TODO: Error checking?

	//Check default extension and append if needed
	if(
		(fpath[ofn.nFileExtension] == '\0' ||
		ofn.nFileExtension == 0) && DefExt\
	){
		char *oldName = strdup(fpath);
		safe_free(fpath);
		asprintf(&fpath, "%s.%s", oldName, DefExt);
		safe_free(oldName);
	}
	
	return fpath;
}
