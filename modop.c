/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "includes.h"              // LOCAL: All includes
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages

// Mod installation operations and the like.

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_CheckCompat
 *  Description:  Checks to make sure that the mod selected is in fact compatible
 *                with the mod loader.
 * =====================================================================================
 */
BOOL Mod_CheckCompat(json_t *root)
{
	// Check mod loader version
	int error_no, ML_Maj = 0, ML_Min = 0, ML_Bfx = 0, ML_Ver = 0;
	char *ML_Str;
	
	// Make sure there even is a version string
	if (
		!root ||
		!json_object_get(root, "ML_Ver") ||
		!json_is_string(json_object_get(root, "ML_Ver"))
	){	
		AlertMsg ("An error has occurred loading mod metadata.\n\n"
			"Contact the mod developer with the following info:\n"
			"No mod loader version string was found.\n"
			"It should be named 'ML_Ver' and be in the format\n"
			"[MAJOR].[MINOR].[BUGFIX]",
			"Invalid JSON");
		return FALSE;	
	}
	
	ML_Str = JSON_GetStr(root, "ML_Ver");
	error_no = sscanf(ML_Str, "%d.%d.%d", &ML_Maj, &ML_Min, &ML_Bfx);
	safe_free(ML_Str);
	
	//Make sure version string is formatted properly
	if (error_no != 3){
		AlertMsg ("This mod is trying to ask for an invalid mod loader version.\n\n"
			"Please contact the mod developer with the following information:\n"
			"ML_Ver string is not in format [MAJOR].[MINOR].[BUGFIX]",
			"Invalid Version Requested");
		return FALSE;
	}

	//Compare versions.
	ML_Ver = ML_Bfx + (ML_Min*0x100) + (ML_Maj*0x10000);
	if(ML_Ver > PROGVERSION){
		char *buf = NULL;
		asprintf (&buf,
			"This mod requires a newer version of the mod loader.\n"
			"You have version %d.%d.%d and need version %d.%d.%d.\n"
			"Obtain an updated copy at\n%s",
			PROGVERSION_MAJOR, PROGVERSION_MINOR, PROGVERSION_BUGFIX,
			ML_Maj, ML_Min, ML_Bfx, PROGSITE);
		AlertMsg (buf, "Outdated Mod Loader");
		safe_free(buf);
		return FALSE;
	}
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_CheckConflict
 *  Description:  Checks if a mod with the current name is already installed
 *                Possible CURRERROR values:
 *                    errNOERR: No conflicts (TRUE)
 *                    errUSR_ABORT: User does not want to install (FALSE)
 *                    errUSR_CONFIRM: User wishes to replace (TRUE)
 *                    errCRIT_DBASE; errCRIT_MALLOC: Critical errors (FALSE)
 *                    errWNG_MODCFG: Use of reserved mod name (FALSE)
 * =====================================================================================
 */ 
BOOL Mod_CheckConflict(json_t *root)
{
	int oldver;
	BOOL FirstInstall = FALSE;
	char *UUID = JSON_GetStr(root, "UUID");
	char *name = JSON_GetStr(root, "Name");
	CURRERROR = errNOERR;

	// Is it on the blacklist?
	// This is more complicated than I had hoped for. Oh well.
	{
		json_t *GameCfg, *GameVerList, *GameVer;
		unsigned int i;
		GameCfg = JSON_Load(CONFIG.GAMECONFIG);

		//Find game version
		GameVerList = json_object_get(GameCfg, "Whitelist");
		json_array_foreach(GameVerList, i, GameVer){
			char *CurrVer = JSON_GetStr(GameVer, "Name");
			json_t *ModList, *CurrMod;
			unsigned int j;

			if(!streq(CurrVer, CONFIG.GAMEVER)){
				// Keep searching
				safe_free(CurrVer);
				continue;
			} 

			//Now iterate through blacklist
			ModList = json_object_get(GameVer, "Mods");
			json_array_foreach(ModList, j, CurrMod){
				const char *CurrItem = json_string_value(CurrMod);
				if(streq(CurrItem, UUID)){
					//A hit!
					char *buf = NULL;
					asprintf (&buf, "%s is implicitly installed in base executable.", name);
					AlertMsg (buf, "Mod Already Installed");
					safe_free(buf);
					CURRERROR = errWNG_MODCFG;
					return FALSE;
				}
			}

			safe_free(CurrVer);
			break;
		}
		
		json_decref(GameCfg);
	}
	
	//Ban if using reserved name MODLOADER@invisibleup as well
	if(streq(UUID, "MODLOADER@invisibleup")){
		AlertMsg(
			"Mod UUID \"MODLOADER@invisibleup\" is "
			"reserved for system use.\nPlease don't use "
			"that UUID or any other \"@invisibleup\" UUID.",
			"Reserved UUID"
		);
		CURRERROR = errWNG_MODCFG;
		return FALSE;
	}
	
	
	// Check for conflicts
	{
		sqlite3_stmt *command;
		const char *query = "SELECT Version FROM Mods WHERE UUID = ? LIMIT 1;";
		CURRERROR = errNOERR;
		
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC)
		) != 0){
			safe_free(UUID);
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		   
		oldver = SQL_GetNum(command);
		//Check that GetNum succeeded
		if(CURRERROR != errNOERR){
			return FALSE;
		}
		if(oldver == -1){
			FirstInstall = TRUE;
		}
		
		if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		safe_free(UUID);
	}

	if (oldver != 0){
		// There IS a version installed
		BOOL MBVal = TRUE; // Message box response
		long ver; // Version required
		
		//Check version number
		ver = JSON_GetInt(root, "Version");
		//Automatically install if not installed at all
		if(FirstInstall == TRUE){
			oldver = ver;
		} 
		
		if(ver > oldver){
			char *buf = NULL;
			asprintf (&buf, 
				"The mod you have selected is a newer version of\n"
				"'%s', which you have already installed.\n"
				"Do you want to upgrade from version %d to version %ld?",
				name, oldver, ver);
			if(buf == NULL){
				safe_free(name);
				CURRERROR = errCRIT_MALLOC;
				return FALSE;
			}
			MBVal = PromptMsg(buf, "Mod Upgrade");
			safe_free(buf);
		}
		else if (ver < oldver){
			char *buf = NULL;
			asprintf (&buf, 
				"The mod you have selected is an OLDER version of\n"
				"'%s', which you have already installed.\n"
				"Do you want to downgrade from version %d to version %ld?",
				name, oldver, ver);
			if(buf == NULL){
				safe_free(name);
				CURRERROR = errCRIT_MALLOC;
				return FALSE;
			}
			MBVal = PromptMsg(buf, "Mod Downgrade");
			safe_free(buf);
		}
		else if (ver == oldver && FirstInstall == FALSE){
			char *buf = NULL;
			asprintf (&buf, "%s is already installed.", name);
			if(buf == NULL){
				safe_free(name);
				CURRERROR = errCRIT_MALLOC; 
				return FALSE;
			}
			AlertMsg (buf, "Mod Already Installed");
			MBVal = FALSE;
			safe_free(buf);
		}
		
		safe_free(name);
		if (MBVal == FALSE){
			//Do not replace
			CURRERROR = errUSR_ABORT;
			return FALSE;
		} 
		else{
			//Do replace
			CURRERROR = errUSR_CONFIRM;
			return TRUE;
		} 
	}
	
	return TRUE; //No conflict
}

//False == ERROR or Dependency issue
//True == No dependency issues (or dependencies, period)!
//TODO: Less confusing name? (This checks for presence of requested mods)
BOOL Mod_CheckDep(json_t *root)
{	
	unsigned int i;
	json_t *value, *deps;
	char *ParentUUID = JSON_GetStr(root, "UUID");
	CURRERROR = errNOERR;
	
	if(ParentUUID == NULL){return FALSE;}
	
	deps = json_object_get(root, "dependencies");
	if(!json_is_array(deps)){
		//No dependencies. Therefore, no issues!
		safe_free(ParentUUID);
		return TRUE;
	}

	json_array_foreach(deps, i, value){
		int ModCount;
		{
			// For each listed dependency, find it in installed mod list
			sqlite3_stmt *command;
			const char *query = "SELECT EXISTS ("
				"SELECT * FROM Mods WHERE UUID = ? AND Version >= ?)";

			char *UUID = JSON_GetStr(value, "UUID");
			unsigned long ver = JSON_GetuInt(value, "Version");
			
			if(SQL_HandleErrors(__FILE__, __LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC) ||
				sqlite3_bind_int(command, 2, ver)
			) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
			   
			ModCount = SQL_GetNum(command);
			if(CURRERROR != errNOERR){return FALSE;}
			
			if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
				CURRERROR = errCRIT_DBASE; return FALSE;
			}
			safe_free(UUID);
		}
		
		if (ModCount == 0){
			//There are missing mods. Roll back and abort.
			sqlite3_stmt *command;
			const char *query = "DELETE FROM Dependencies WHERE ParentUUID = ?;";
			if(SQL_HandleErrors(__FILE__, __LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
				sqlite3_bind_text(command, 1, ParentUUID, -1, SQLITE_STATIC)
			) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
			) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
			) != 0){
				CURRERROR = errCRIT_DBASE; return FALSE;
			}
			command = NULL;
			safe_free(ParentUUID);
			return FALSE;
		}
		
		else{
			//Insert that dependency into the table for uninstallation purposes
			sqlite3_stmt *command;
			const char *query = "INSERT INTO Dependencies "
				"('ChildUUID', 'ParentUUID')  VALUES (?, ?);";
			char *UUID = JSON_GetStr(value, "UUID");
			if(SQL_HandleErrors(__FILE__, __LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC) ||
				sqlite3_bind_text(command, 2, ParentUUID, -1, SQLITE_STATIC)
			) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
			) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
			) != 0){
				CURRERROR = errCRIT_DBASE; return FALSE;
			}
			command = NULL;
			safe_free(UUID);
		}
	}
	safe_free(ParentUUID);
	return TRUE;
}

//Alert helper function
void Mod_CheckDepAlert(json_t *root){
	char *message = NULL;
	unsigned int messagelen;
	unsigned int i;
	json_t *value, *deps;
	CURRERROR = errNOERR;
	
	{
		char *parName = JSON_GetStr(root, "Name");
		char *parAuth = JSON_GetStr(root, "Author");
		unsigned long parVer = JSON_GetuInt(root, "Version");
		messagelen = asprintf(&message,
			"The mod you are trying to install,\n"
			"%s version %ld by %s, requires some mods\n"
			"that you do not have installed:\n\n",
			parName, parVer, parAuth
		);
		safe_free(parName);
		safe_free(parAuth);
	}
	
	deps = json_object_get(root, "dependencies");
	if (!json_is_array(deps)){
		//No dependencies.
		safe_free(message);
		return;
	}
	
	json_array_foreach(deps, i, value){
		int ModCount;
		
		{
			sqlite3_stmt *command;
			const char *query = "SELECT COUNT(*) FROM Mods "
			                    "WHERE UUID = ? AND Version >= ?";
			char *UUID = JSON_GetStr(value, "UUID");
			unsigned long ver = JSON_GetuInt(value, "Version");
			
			if(SQL_HandleErrors(__FILE__, __LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
				sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC) ||
				sqlite3_bind_int(command, 2, ver)
			) != 0){
				CURRERROR = errCRIT_DBASE;
				safe_free(UUID);
				safe_free(message);
				return;
			}
			   
			ModCount = SQL_GetNum(command);
			if(CURRERROR != errNOERR){return;}
			
			if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
				CURRERROR = errCRIT_DBASE;
				safe_free(UUID);
				safe_free(message);
				return;
			}
			safe_free(UUID);
		}
		
		if (ModCount == 0){ //If mod not installed
			char *name = JSON_GetStr(value, "Name");
			char *auth = JSON_GetStr(value, "Author");
			unsigned long ver = JSON_GetuInt(value, "Version");
			char *out = NULL;
			
			asprintf(&out, "- %s version %lu by %s\n", name, ver, auth);
			if(out == NULL){
				CURRERROR = errCRIT_MALLOC;
				safe_free(name);
				safe_free(auth);
				return;
			}
			
			while(strlen(message) + strlen(out) + 1 >= messagelen){
				char *newmessage = NULL;
				messagelen *= messagelen;
				newmessage = realloc(message, messagelen);
				if(newmessage == NULL){
					CURRERROR = errCRIT_MALLOC;
					safe_free(message);
					safe_free(name);
					safe_free(auth);
					safe_free(out);
					return;
				}
				message = newmessage;
			}
			strncat(message, out, messagelen - strlen(message));
			
			safe_free(name);
			safe_free(auth);
			safe_free(out);
		}
	}
	
	AlertMsg(message, "Other Mods Required");
	safe_free(message);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ModOp_Clear
 *  Description:  Creates an clear space marker given a ModSpace struct containing a 
 *                start address, end address and file. Very dumb in operation; make sure
 *                space is indeed ready to be cleared first.
 * =====================================================================================
 */
BOOL ModOp_Clear(struct ModSpace *input, const char *ModUUID){
	unsigned char *pattern = NULL;
	int patternlen = 0;
	
	char *FilePath = NULL;
	
	struct ModSpace parentSpace = {0};
	int FileLen;
	
	BOOL retval = FALSE;
	CURRERROR = errNOERR;

	FilePath = File_GetPath(input->FileID);
	if(strndef(FilePath)){return FALSE;}
	
	// Find space that this space would be contained within
	parentSpace = Mod_FindParentSpace(input);
	
	// Convert parentSpace from PE offsets if needed
	parentSpace.Start = File_PEToOff(FilePath, parentSpace.Start);
	parentSpace.End = File_PEToOff(FilePath, parentSpace.End);

	// Are we actually occupying multiple spaces?
	if(parentSpace.Valid == FALSE) {

		// We could be. Which ones?
		sqlite3_stmt *command;
		const char *query =
			"SELECT ID, Start, End FROM Spaces "
			"WHERE (End >= ? AND Start <= ?) AND "
			"File = ? AND UsedBy IS NULL "
			"ORDER BY Start ASC";
		json_t *out, *row;
		size_t i;
		
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_int(command, 1, input->Start) ||
			sqlite3_bind_int(command, 2, input->End) ||
			sqlite3_bind_int(command, 3, input->FileID)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			goto ModOp_Clear_Return;
		}
		
		out = SQL_GetJSON(command);
		if(json_array_size(out) == 0){
			// There's no spaces. Like, at all. No good.
			CURRERROR = errWNG_MODCFG;
			goto ModOp_Clear_Return;
		}
		sqlite3_finalize(command);
		
		//Make our own parent space
		memcpy(&parentSpace, input, sizeof(struct ModSpace));
		
		// What the lowest space's Start? That's Start.
		row = json_array_get(out, 0);
		parentSpace.Start = JSON_GetuInt(row, "Start");
		
		// What's the highest space's End? That's End.
		row = json_array_get(out, json_array_size(out) - 1);
		parentSpace.End = JSON_GetuInt(row, "End");
		
		// Mark all of the overlapping spaces as used and invalid.
		// (This isn't a very efficient way to do this, but it's easy.)
		json_array_foreach(out, i, row){
			char *ID = JSON_GetStr(row, "ID");
			Mod_ClaimSpace(ID, ModUUID);
			safe_free(ID);
		}
		
		// Make a new add space overlapping all of them
		if(!Mod_MakeSpace(&parentSpace, ModUUID, "Add")){
			goto ModOp_Clear_Return;
		}

		json_decref(out);
		
		// Now continue as if we had a parent space this entire time.
	}
	
	{
		sqlite3_stmt *command;
		const char *query = "UPDATE Spaces SET Type = 'Clear' WHERE ID = ? AND Version = ?";

		Mod_SpliceSpace(&parentSpace, input, ModUUID, input->PatchID);
		if(CURRERROR != errNOERR){goto ModOp_Clear_Return;}

		// I guess SpliceSpace screws this up
		parentSpace.Start = File_PEToOff(FilePath, parentSpace.Start);
		parentSpace.End = File_PEToOff(FilePath, parentSpace.End);

		input->Start = parentSpace.Start;
		input->End = parentSpace.End;
		input->Len = input->End - input->Start;
		if(parentSpace.ID != input->ID){
			safe_free(input->ID);
			input->ID = parentSpace.ID; // strdup not needed
		}
		
		// Mark space as clear
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, input->ID, -1, SQLITE_STATIC) ||
			sqlite3_bind_int(command, 2, Mod_GetVerCount(input->ID))
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			goto ModOp_Clear_Return;
		}

		// Nothing should have changed, data-wise, so we don't need revert entry.
	}
	
	// Create revert table entry
	Mod_CreateRevertEntry(input);
	
	// Write pattern depending on file contents
	if(File_IsPE(FilePath)){
		// Use UD2 (x86 undefined operation)
		unsigned short UD2 = 0x0F0B;
		patternlen = 2;
		pattern = malloc(2);
		if (!pattern) {
			CURRERROR = errCRIT_MALLOC;
			return retval;
		}
		memcpy(pattern, &UD2, 2);
	} else if (CURRERROR == errNOERR) {
		// Use generic 0xFF
		unsigned char EMPTY = 0xFF;
		patternlen = 1;
		pattern = malloc(1);
		if (!pattern) {
			CURRERROR = errCRIT_MALLOC;
			return retval;
		}
		memcpy(pattern, &EMPTY, 1);
	} 
	
	if (CURRERROR != errNOERR) {
        safe_free(pattern);
		return retval;
	}

	FileLen = filesize(FilePath);

	if(input->Start == 0 && input->End == FileLen){
		// Entire file is cleared. Delete it!
		Mod_MakeSpace(input, ModUUID, "Delete");
		File_Delete(FilePath);

	} else if(input->FileID != 0){
		// Write data in place (so if something did use this, it would blow up)
		int handle = File_OpenSafe(FilePath, _O_BINARY|_O_RDWR);
		if (handle == -1) {
			CURRERROR = errWNG_BADFILE;
			retval = FALSE;
			goto ModOp_Clear_Return;
		}
		File_WritePattern(handle, input->Start, pattern, patternlen, input->Len);
		close(handle);
	}
	safe_free(pattern);
	retval = TRUE;
	
ModOp_Clear_Return:
	safe_free(FilePath);
	//parentSpace.ID is in use!
	//parentSpace.bytes is also input.Bytes!

	return retval;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ModOp_Add
 *  Description:  Add operation for mod installation.
 *                Applies to "Add", "Repl" and "Copy" operations.
 *                Finds free space in given file and writes given bytes to it.
 * =====================================================================================
 */
BOOL ModOp_Add(struct ModSpace *input, const char *ModUUID){

	//"Reserve" input space. (ADD is just RESERVE with bytes attached)
	if(!ModOp_Reserve(input, ModUUID)){
		return FALSE;	
	};
	
	//Create revert table entry
	Mod_CreateRevertEntry(input);
	
	if(input->FileID != 0){
		//Only write if not using memory pseudofile
		int handle = -1;
		char *FilePath = NULL;

		FilePath = File_GetPath(input->FileID);
		if(strndef(FilePath)){return FALSE;}

		handle = File_OpenSafe(FilePath, _O_BINARY|_O_RDWR);
		File_WriteBytes(handle, input->Start, input->Bytes, input->Len);
		close(handle);

		safe_free(FilePath);
	}
	return TRUE;
}

// Returns an ID dependant on the value of the equation.
// Will be value converted to hex unless sole contents of
// eq are "$ Start.XXXX". or "$ End.XXXX"

char * Mod_GetPatchInfo_SetID(const char *eq, unsigned int eqVal)
{
	char *pch = NULL;
	
	pch = strstr(eq, "$ Start.");
	if(pch == eq){
		pch += strlen("$ Start.");
		return strdup(pch);
	}

	pch = strstr(eq, "$ End.");
	if(pch == eq){
		pch += strlen("$ End.");
		return strdup(pch);
	}

	asprintf(&pch, "%X", eqVal);
	return pch;
}

// Set Start, End, and ID given a Start and End
// Reusable for SrcStart and SrcEnd
// Yeah, there's way too many arguments here
BOOL Mod_GetPatchInfo_SetRange(
	const char *StartEq, const char *EndEq, const char *PatchID,
	const char *FilePath, const char *FileName, const char *FileType, const char *ModPath,
	int *Start, int *End, char **ID, BOOL SetID // Output (excluding BOOL)
){
	int FileID = File_GetID(FileName);
	//char *FilePath = NULL;
	//asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);

	// Check and handle a whole-file op case
	if(strndef(StartEq)){

		*Start = 0;
		*End = filesize(FilePath);

		//AlertMsg("WHOLE-FILE OP", "DEBUG");

		// Set ID if unset
		if(strndef(PatchID) && SetID){
			asprintf(ID, "%d-ALL", FileID);
		} else if (SetID){
			*ID = strdup(PatchID);
		}

		return TRUE;

	} else if (strndef(EndEq)) {
		// Start defined, but end not.
		// Start might be a Patch ID to replace

		struct ModSpace clone;

		// Set Start to patch's start, End to patch's end, ID to patch's ID
		clone = Mod_GetPatch(StartEq); // In case of classic style Start
		*Start = clone.Start;
		*End = clone.End;
        
        if(!clone.Valid){
			AlertMsg(
				"End (or SrcEnd) is not defined and Start (or SrcStart) is not a reference "
				"to an existing patch.", "Mod Config Error"
			);
            return FALSE;
        }

		if(SetID && strndef(PatchID)){
			*ID = strdup(clone.ID);
		} else if (SetID) {
			*ID = strdup(PatchID);
		}

		safe_free(clone.ID);
		safe_free(clone.PatchID);
		return TRUE;

	} else {
		// Parse both expressions
		*Start = Eq_Parse_uInt(StartEq, ModPath, TRUE);
		*End = Eq_Parse_uInt(EndEq, ModPath, TRUE);

		if(CURRERROR != errNOERR){
			return FALSE;
		}

		// Convert if FileType is set to "PE"
		if(streq(FileType, "PE") && FileID != 0){
			//Convert memory address to file offset
			*Start = File_PEToOff(FilePath, *Start);
			*End = File_PEToOff(FilePath, *End);
		}
		
		// Set ID if unset
		if(SetID && strndef(PatchID)){
			*ID = Mod_GetPatchInfo_SetID(StartEq, *Start);
		} else if (SetID) {
			*ID = strdup(PatchID);
		}

		if(FileID != 0){
			if(*Start > *End){
				AlertMsg(
					"(Src)Start is greater than (Src)End.",
					"Mod Config Error"
				);

				CURRERROR = errWNG_MODCFG;
				return FALSE;
			} else if (filesize(FilePath) < *End){
				// Set to end of file. It'll be fine...
				*End = filesize(FilePath);
				/*AlertMsg(
					"(Src)End is past end of file.",
					"Mod Config Error"
				);

				CURRERROR = errWNG_MODCFG;
				safe_free(FilePath);
				return FALSE;*/
			} else if (0 > *Start){
				// Set to 0
				*Start = 0;
				/*AlertMsg(
					"(Src)Start is less than 0.",
					"Mod Config Error"
				);

				CURRERROR = errWNG_MODCFG;
				safe_free(FilePath);
				return FALSE;*/
			}
		}

		return TRUE;
	}
}

//On failure, output guaranteed to be returned without calls to free() being required
//Fills a ModSpace with all the information needed to execute a patch
//
//General flow is this: (may be inaccurate)
//	- Fill FileID
//	- Fill Start/End either from number or UUID
//	- If no ID given or mode is REPL:
//		- If Start is numeric, set ID to hex representation of Start
//		- If Start is string, set ID to Start
//	- If mode is COPY or MOVE:
//		- Fill SrcStart, SrcEnd, SrcFile
//		- Set Len = SrcEnd - SrcStart
//	- If mode is ADD or REPL:
//		- Fill Bytes with given value
//		- Set Len to length of Bytes
struct ModSpace Mod_GetPatchInfo(
	json_t *patchCurr,
	const char *ModPath,
	const char *ModUUID,
	size_t PatchCount
){
	struct ModSpace input = {0}; //Return value
	char *Mode = NULL; //Patch mode

	char *FileName = NULL;
	char *FilePath = NULL;
	char *FileType = NULL;

	char *StartEq = NULL;
	char *EndEq = NULL;

	CURRERROR = errNOERR;
	input.Valid = FALSE;
	
	// Get Mode
	Mode = JSON_GetStr(patchCurr, "Mode");
	if(strndef(Mode)){
		// Mode was not found
		AlertMsg("`Mode` not defined for patch.", "JSON Error");
		goto Mod_GetPatchInfo_Failure;
	}
	
	/// Set FileID and FileType
	// (By this function FileName is guaranteed to be good)
	FileName = JSON_GetStr(patchCurr, "File");
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);

	input.FileID = File_GetID(FileName);
	FileType = JSON_GetStr(patchCurr, "FileType");

	// Init Space IDs to whatever it's defined as, or autogenerated
	input.PatchID = JSON_GetStr(patchCurr, "ID");
	if(strndef(input.PatchID)){
		// Make generic PatchID (for sanity's sake)
		asprintf(&input.PatchID, "Patch-%zu.%s", PatchCount, ModUUID);
	}
	input.ID = NULL;
	 
	/// Set Start, End
	StartEq = JSON_GetStr(patchCurr, "Start");
	EndEq = JSON_GetStr(patchCurr, "End");
	Mod_Install_VarRepatchFromExpr(StartEq, ModPath, PatchCount);
	Mod_Install_VarRepatchFromExpr(EndEq, ModPath, PatchCount);

	if(!Mod_GetPatchInfo_SetRange(
		StartEq, EndEq, input.PatchID,
		FilePath, FileName, FileType, ModPath,
		&(input.Start), &(input.End), &(input.ID), TRUE
	)){
		goto Mod_GetPatchInfo_Failure;
	}

	// Set Len if exists
	if(Mod_PatchKeyExists(patchCurr, "Len", FALSE)){
		
		char *LenEq = JSON_GetStr(patchCurr, "Len");
		input.Len = Eq_Parse_uInt(LenEq, ModPath, FALSE);
		Mod_Install_VarRepatchFromExpr(LenEq, ModPath, PatchCount);
		safe_free(LenEq);

		if(CURRERROR != errNOERR){
			goto Mod_GetPatchInfo_Failure;
		}

	} else {
		//input.Len = 0;  // Why did I do this?
        input.Len = input.End - input.Start;
	}
	
	//Make a new file if the file doesn't exist.
	if(!File_Exists(FilePath, FALSE, FALSE)){
		File_Create(FilePath, input.End);
	}
	
	///Set Bytes, Len, SrcStart, SrcEnd (if applicable)
	//Copy/Move
	if(strieq(Mode, "Move")|| strieq(Mode, "Copy")){
		char *SrcFile = NULL;
		char *SrcPath = NULL;
		char *SrcLoc = NULL;
		char *SrcType = NULL;
		char *SrcStartEq = NULL;
		char *SrcEndEq = NULL;
	
		//Get source file
		SrcFile = JSON_GetStr(patchCurr, "SrcFile");
		SrcLoc = JSON_GetStr(patchCurr, "SrcFileLoc");
		SrcType = JSON_GetStr(patchCurr, "SrcFileType");
		SrcStartEq = JSON_GetStr(patchCurr, "SrcStart");
		SrcEndEq = JSON_GetStr(patchCurr, "SrcEnd");

		Mod_Install_VarRepatchFromExpr(SrcStartEq, ModPath, PatchCount);
		Mod_Install_VarRepatchFromExpr(SrcEndEq, ModPath, PatchCount);

		if(strndef(SrcFile)){
			// Source file is current file
			safe_free(SrcFile);
			SrcFile = strdup(FileName);
		}
		
		if(strieq(SrcLoc, "Mod")){
			//Mod archive
			asprintf(&SrcPath, "%s%s", ModPath, SrcFile);
		} else {
			//Game installation directory
			asprintf(&SrcPath, "%s/%s", CONFIG.CURRDIR, SrcFile);
		}
		safe_free(SrcLoc);

		// Comfirm SrcPath is valid
		if(!File_Exists(SrcPath, FALSE, TRUE)){
			AlertMsg("SrcFile does not exist!", "Mod configuration error");
			goto Mod_GetPatchInfo_Src_Failure;
		}
		
		// Set SrcStart/SrcEnd
		if(!Mod_GetPatchInfo_SetRange(
			SrcStartEq, SrcEndEq, input.ID,
			SrcPath, SrcFile, SrcType, ModPath,
			&(input.SrcStart), &(input.SrcEnd), NULL, FALSE
		)){
			goto Mod_GetPatchInfo_Src_Failure;
		}

		// Set length if needed
		if(input.Len == 0){
			input.Len = input.SrcEnd - input.SrcStart;
		}
		
		// Allocate space for bytes
		input.Bytes = calloc(input.Len, sizeof(char));
		if(input.Bytes == NULL){
			CURRERROR = errCRIT_MALLOC;
			goto Mod_GetPatchInfo_Src_Failure;
		}
		
		// Set value of Bytes to value of bytes from SrcStart to SrcEnd
		{
			int fhandle = File_OpenSafe(SrcPath, _O_BINARY | _O_RDONLY);
			if(fhandle == -1){
				safe_free(input.Bytes);
				CURRERROR = errCRIT_FUNCT;
				goto Mod_GetPatchInfo_Src_Failure;
			}
			
			lseek(fhandle, input.SrcStart, SEEK_SET);
			if (read(fhandle, input.Bytes, input.Len) != input.Len){
				safe_free(input.Bytes);
				CURRERROR = errCRIT_FUNCT;
				goto Mod_GetPatchInfo_Src_Failure;
			}
			close(fhandle);
		}

	Mod_GetPatchInfo_Src_Failure:
		safe_free(SrcStartEq);
		safe_free(SrcEndEq);
		safe_free(SrcPath);
		safe_free(SrcFile);
		safe_free(SrcType);
	}

	if(CURRERROR != errNOERR){
		goto Mod_GetPatchInfo_Failure;
	}
	
	// Add, Repl
	if(strieq(Mode, "Add") || strieq(Mode, "Repl")){
		char *ByteStr = NULL, *ByteMode = NULL;
		
		// Check if ByteMode and ByteStr exist
		if(
			!Mod_PatchKeyExists(patchCurr, "AddType", TRUE) ||
			!Mod_PatchKeyExists(patchCurr, "Value", TRUE)
		){
			CURRERROR = errWNG_MODCFG;
			goto Mod_GetPatchInfo_Failure;
		}
		
		// Get Byte Mode and String.
		ByteStr = JSON_GetStr(patchCurr, "Value");
		ByteMode = JSON_GetStr(patchCurr, "AddType");
		
		////  Possible byte modes:
		//  "Bytes"       - Raw data in hex form to be copied exactly.
		//  "VarValue"    - Contents of givenmod loader variable 
		//  "Expression"  - Parse given expression as uInt32
		
		if (strieq(ByteMode, "Bytes")){
			// Convert the hex value of Value to bytes
			input.Bytes = Hex2Bytes(ByteStr, &input.Len);
			
		} else if (strieq(ByteMode, "Expression")){
			unsigned int val;

			// Find address/length of ID
			val = Eq_Parse_uInt(ByteStr, ModPath, FALSE);
			Mod_Install_VarRepatchFromExpr(ByteStr, ModPath, PatchCount);
			
			// Patch size MUST be 4 bytes.or less
			if(input.Len == 0){
				input.Len = 4;
			} else {
				input.Len = MIN(input.Len, sizeof(unsigned int));
			}
			
			// Allocate space for bytes
			input.Bytes = calloc(input.Len, sizeof(char));
			if(input.Bytes == NULL){
				CURRERROR = errCRIT_MALLOC;
				goto Mod_GetPatchInfo_Failure;
			}
			
			// Set input.Bytes to value of start
			memcpy(input.Bytes, &val, input.Len);
			
		} else if (strieq(ByteMode, "VarValue")){
			struct VarValue var = {0};
			
			var = Var_GetValue_SQL(ByteStr);
			if(var.type == INVALID){
				char *msg = NULL;
				asprintf(
					&msg, "Variable %s not defined.",
					ByteStr
				);
				AlertMsg(msg, "JSON Error");
				safe_free(msg);
				CURRERROR = errWNG_MODCFG;
				goto Mod_GetPatchInfo_Failure;
			}
			
			input.Len = Var_GetLen(&var);
			memcpy(&input.Bytes, &var.raw, input.Len);
			
			Var_Destructor(&var);
		}
		safe_free(ByteStr);
		safe_free(ByteMode);
	}

	input.Valid = TRUE; //We reached end naturally, success
	goto Mod_GetPatchInfo_Return;
	
Mod_GetPatchInfo_Failure:
	safe_free(input.ID);
	safe_free(input.Bytes);
	safe_free(input.PatchID);
	
Mod_GetPatchInfo_Return:
	safe_free(StartEq);
	safe_free(EndEq);
	safe_free(Mode);
	safe_free(FileName);
	safe_free(FilePath);
	return input;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ModOp_Reserve
 *  Description:  Finds and creates an Add space for the given input.
 * =====================================================================================
 */
BOOL ModOp_Reserve(struct ModSpace *input, const char *ModUUID){
	struct ModSpace FreeSpace;
	sqlite3_stmt *command;
	const char *query = "UPDATE Spaces SET Type = 'Add' WHERE ID = ? AND Version = ?";

	//Find some free space to put it in
	FreeSpace = Mod_FindSpace(input, TRUE);
	
	if(FreeSpace.Valid){
		//Alter input to match found space
		input->Start = MAX(input->Start, FreeSpace.Start);
		input->End = MIN(input->Start + input->Len, FreeSpace.End);
	}
	
	if(!FreeSpace.Valid) {	//If no free space
		if (CURRERROR == errNOERR) {
			AlertMsg(
				"There is not enough space cleared in the file to install\n"
				"this mod. Try removing some mods or installing some\n"
				"space-clearing mods to help make room.\n"
				"The installation cannot continue.",
				"Mod Installation Error"
			);
		}
		safe_free(FreeSpace.ID);
		safe_free(FreeSpace.Bytes);
		safe_free(FreeSpace.PatchID);
		return FALSE;
	}
	
	// Splice input into FreeSpace
	Mod_SpliceSpace(&FreeSpace, input, ModUUID, input->PatchID);

	if (CURRERROR == errNOERR) { ErrNo2ErrCode(); }
	if (CURRERROR != errNOERR) {
		safe_free(FreeSpace.ID);
		safe_free(FreeSpace.Bytes);
		safe_free(FreeSpace.PatchID);
		return FALSE;
	}
	
	// Turn our child into an add space
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, input->ID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 2, Mod_GetVerCount(input->ID))
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		safe_free(FreeSpace.ID);
		safe_free(FreeSpace.Bytes);
		safe_free(FreeSpace.PatchID);
		return FALSE;
	}

	// Add a Start variable.
	/*{
		struct VarValue startvar = {0};
		startvar.type = uInt32;
		startvar.mod = strdup(ModUUID);
		startvar.uInt32 = input->Start;
		startvar.persist = FALSE;
		asprintf(&(startvar.UUID), "Start.%s.%s", input->ID, ModUUID);

		Var_MakeEntry(startvar);
		Var_Destructor(&startvar);
	}*/
	
	safe_free(FreeSpace.ID);
	safe_free(FreeSpace.Bytes);
	safe_free(FreeSpace.PatchID);
	return TRUE;
}

// Convienece wrapper to test if a value exists in a patch.
BOOL Mod_PatchKeyExists(json_t *patchCurr, const char *KeyName, BOOL ShowAlert)
{
	char *temp;
	BOOL retval = TRUE;
		
	// Make sure KeyName exists
	temp = JSON_GetStr(patchCurr, KeyName);
	if(strndef(temp)){
		if(ShowAlert){
			char *msg = NULL;
			asprintf(&msg, "'%s' not defined for patch.", KeyName);
			AlertMsg(msg, "JSON Error");
			safe_free(msg);
		}
		retval = FALSE;
	}
	safe_free(temp);
	
	return retval;
}

// Replace the info with the start/end of the given UUID
BOOL Mod_FindUUIDLoc(int *start, int *end, const char *UUID)
{
	sqlite3_stmt *command;
	json_t *out, *row;
	const char *query = "SELECT Start, End FROM Spaces "
		     "WHERE ID = ? AND UsedBy IS NULL LIMIT 1;";
		     
	CURRERROR = errNOERR;

	// Get the list
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	
	out = SQL_GetJSON(command);
	row = json_array_get(out, 0);
	
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}
	
	// Make sure list is not empty
	if(json_array_size(out) == 0){
		char *errormsg = NULL;
		asprintf(
			&errormsg, "%sThe UUID %s is not a known UUID from\n"
			           "the game or any installed mods.",
			errormsg_ModInst_CantCont, UUID
		);
		AlertMsg(errormsg, "Mod Metadata Error");
		safe_free(errormsg);
		
		CURRERROR = errWNG_MODCFG;
		return FALSE;
	}
	
	// Parse the list
	*start = JSON_GetuInt(row, "Start");
	*end = JSON_GetuInt(row, "End");

	json_decref(out);
	return TRUE;
}

// Fills the 'Start' and 'End' members of 'input' with the location of the
// given UUID. Errors if the UUID is from a different file than input.FileID.
// If 'IsSrc' is true, fills 'SrcStart' and 'SrcEnd' instead, checking 'SrcFile'
BOOL Mod_PatchFillUUID(
	struct ModSpace *input, BOOL IsSrc,
	const char *FileType, const char *FileName,
	const char *FilePath, const char *UUID
){
	char *UUIDFile = NULL;
	BOOL resultcode;
	int tempStart, tempEnd;
	
	//Get owner of UUID
	UUIDFile = File_FindPatchOwner(UUID);
	
	//Check if files are the same
	if(strneq(UUIDFile, FileName)){
		AlertMsg(
			"UUID in 'Start' is from different file than 'File'",
			"JSON Error"
		);
		safe_free(UUIDFile);
		return FALSE; //Different file. That's no good.
	}
	safe_free(UUIDFile);
	
	//Replace Start and End with UUID's start/end
	resultcode = Mod_FindUUIDLoc(&tempStart, &tempEnd, UUID);
	if(resultcode == FALSE){
		return FALSE; //Database error. That's no good.
	}
	
	//Check if SrcType is set to "PE"
	if(
		streq(FileType, "PE") &&
		strneq(FileName, ":memory:")
	){
		//Convert file offset to memory address
		tempStart = File_OffToPE(FilePath, tempStart);
		tempEnd = File_OffToPE(FilePath, tempEnd);
	}
	
	if(IsSrc){
		input->SrcStart = tempStart;
		input->SrcEnd = tempEnd;
	} else {
		input->Start = tempStart;
		input->End = tempEnd;
	}
	
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_MangleUUID
 *  Description:  Converts the given UUID into a path-safe name
 * =====================================================================================
 */
char * Mod_MangleUUID(const char *UUID)
{
	char *result = strdup(UUID);
	const char blacklist[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
		17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
		'<', '>', ':', '"', '/', '\\', '|', '?', '*', '\0'
	}; //This is technically a valid string. That makes things nice.
	char *ptr = result;
	
	ptr = strpbrk (UUID, blacklist);
	while (ptr != NULL){
		*ptr = '_';
		ptr = strpbrk (UUID, blacklist);
	}
	
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_Verify
 *  Description:  Calls various other functions to make sure mod is OK to install
 * =====================================================================================
 */
BOOL Mod_Verify(json_t *root)
{
	if (Mod_CheckCompat(root) == FALSE){
		return FALSE;
	}
	if (Mod_CheckConflict(root) == FALSE){
		return FALSE;
	}
	if (Mod_CheckDep(root) == FALSE){
		if(CURRERROR == errNOERR){
			Mod_CheckDepAlert(root);
		}
		return FALSE;
	}
	return TRUE;
}

BOOL Mod_Install_VarRepatchFromExpr(
	const char *ExprStr,
	const char *ModPath,
	size_t PatchNo
){
	char **Vars = NULL;
	int i = 0;
	int allocCnt = 64;
	char *pch, *varname, *space;

	if(strndef(ExprStr)){
		return TRUE;
	}

	Vars = calloc(allocCnt, sizeof(char *));
	if (!Vars) {
		CURRERROR = errCRIT_MALLOC;
		return FALSE;
	}

	pch = strstr(ExprStr, "$ ");
	while(pch != NULL){

		varname = pch + strlen("$ ");
		space = strchr(varname, ' ');
		if(space == NULL){
			// End of string
			space = pch + strlen(pch);
		}

		Vars[i] = calloc(space - varname + 1, sizeof(char));
		strncpy(Vars[i], varname, (space - varname));

		i += 1;
		if(i > allocCnt){
			allocCnt *= 2;
			Vars = realloc(Vars, allocCnt * sizeof(char *));
			if(Vars == NULL){
				CURRERROR = errCRIT_MALLOC;
				return FALSE;
			}
		}

		pch = strstr(pch + 1, "$ ");
	}
	
	i -= 1; // We'll be over by one just because of how the loop works...
	
	while(i >= 0) {
		sqlite3_stmt *command;
		char *query = "INSERT INTO VarRepatch "
					  "(Var, ModPath, Patch, OldVal) VALUES (?, ?, ?, ?);";
		struct VarValue result = Var_GetValue_SQL(Vars[i]);
		
		//Compose SQL
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, Vars[i], -1, SQLITE_STATIC) ||
			sqlite3_bind_text(command, 2, ModPath, -1, SQLITE_STATIC) ||
			sqlite3_bind_int(command, 3, PatchNo)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

		//Swap out OldVal for appropriate type.
		switch(result.type){
		case IEEE64:
			sqlite3_bind_double(command, 4, result.IEEE64); break;
		case IEEE32:
			sqlite3_bind_double(command, 4, result.IEEE32); break;
		case Int32:
			sqlite3_bind_int(command, 4, result.Int32); break;
		case Int16:
			sqlite3_bind_int(command, 4, result.Int16); break;
		case Int8:
			sqlite3_bind_int(command, 4, result.Int8); break;
		case uInt32:
		case uInt32Pointer:
			sqlite3_bind_int(command, 4, result.uInt32); break;
		case uInt16:
			sqlite3_bind_int(command, 4, result.uInt16); break;
		case uInt8:
			sqlite3_bind_int(command, 4, result.uInt8); break;
		default:
			sqlite3_bind_int(command, 4, 0); break;
		}
			
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

		Var_Destructor(&result);
		safe_free(Vars[i]);
		i -= 1;
	}

	safe_free(Vars);
	return TRUE;
}

// Handles mini-patches 
BOOL Mod_Install_Minipatch(
	json_t *miniArray,
	struct ModSpace *input,
	const char *ModPath,
	const char *ModUUID,
	size_t PatchNo
){
	json_t *miniCurr;
	size_t j;
	BOOL retval = TRUE;

	json_array_foreach(miniArray, j, miniCurr){
		char *CurrVarStr = NULL;
		char *ExprStr = NULL;
		char *FilePath = NULL;
		char *PatchMode = NULL;
		int CurrVarLen;
		int CurrVarLoc;
		struct VarValue CurrVar;
		struct ModSpace CurrInput;
		
		// Retrieve variable 
		memset(&CurrInput, 0, sizeof(CurrInput));
		CurrVarStr = JSON_GetStr(miniCurr, "Var");
		CurrVar = Var_GetValue_SQL(CurrVarStr);
		ExprStr = JSON_GetStr(miniCurr, "Expression");
		PatchMode = JSON_GetStr(miniCurr, "PatchMode"); // Hack I just added...
		FilePath = File_GetPath(input->FileID);

		if(strndef(FilePath)){continue;}

		// Valid PatchMode values (4 bytes only!)
		// REL: Relative to end of data
		// ADD: Add to current value of section
		
		CurrVarLen = JSON_GetuInt(miniCurr, "MaxLen");
		CurrVarLoc = JSON_GetuInt(miniCurr, "Pos");
		
		// Determine actual size
		CurrVarLen = MIN(CurrVarLen, Var_GetLen(&CurrVar));
		
		if(CurrVarLen == 0 && (!CurrVarStr || strneq(CurrVarStr, ""))){
			goto MiniPatch_CleanUp;
		} else if (strneq(ExprStr, "")){
			CurrVarLen = 4;
		}
		
		// Create ModSpace
		// This memcpy includes pointers to allocated memory!
		memcpy(&CurrInput, input, sizeof(struct ModSpace)); 
		asprintf(&CurrInput.ID, "%s/%zu", input->ID, j);
		
		CurrInput.Start += CurrVarLoc;
		CurrInput.End = CurrInput.Start + CurrVarLen;
		CurrInput.Len = CurrVarLen;
		
		CurrInput.Bytes = malloc(CurrVarLen);

		if(strndef(ExprStr)){
			memcpy(CurrInput.Bytes, &CurrVar.raw[0], CurrVarLen);
		} else {
			unsigned int val = Eq_Parse_uInt(ExprStr, ModPath, FALSE);
			memcpy(CurrInput.Bytes, &val, CurrVarLen);
		}

		// Extremely hacky way of making sure CALL/JMP operators are relative
		// (E8 = CALL, E9 = JMP)
		if(	strieq(PatchMode, "rel") && CurrInput.Len == 4	){ 
			uint32_t OpLoc = CurrInput.Start - 1; // Call/Jmp op
			uint32_t RelOffset = File_OffToPE(FilePath, CurrInput.End); // Next address
			uint8_t OpByte;
			int handle;
			uint32_t AbsPos;
			
			// We need to somehow open and read a single byte from the EXE.
			handle = File_OpenSafe(FilePath, _O_BINARY | _O_RDONLY);
			lseek(handle, OpLoc, SEEK_SET);
			read(handle, &OpByte, 1);
			close(handle);

			memcpy(&AbsPos, CurrInput.Bytes, 4);
			AbsPos -= RelOffset;
			memcpy(CurrInput.Bytes, &AbsPos, 4);

		}

		if( strieq(PatchMode, "add") && CurrInput.Len == 4 ){
			int handle;
			uint32_t OldVal;
			uint32_t NewVal;
			
			// We need to somehow open and read a single byte from the EXE.
			handle = File_OpenSafe(FilePath, _O_BINARY | _O_RDONLY);
			lseek(handle, CurrInput.Start, SEEK_SET);
			read(handle, &OldVal, 4);
			close(handle);

			memcpy(&NewVal, CurrInput.Bytes, 4);
			NewVal += OldVal;
			memcpy(CurrInput.Bytes, &NewVal, 4);
		}

		
		CurrInput.Valid = TRUE;
		Var_Destructor(&CurrVar);
		
		// Splice value in
		ModOp_Clear(&CurrInput, ModUUID);
		ModOp_Add(&CurrInput, ModUUID);
		safe_free(CurrInput.Bytes);

		// Create VarWritePos entries
		if(strndef(ExprStr)){
			asprintf(&ExprStr, "%% %s", CurrVarStr);
		}

		retval &= Mod_Install_VarRepatchFromExpr(ExprStr, ModPath, PatchNo);

		//safe_free(CurrInput.PatchID);
		safe_free(FilePath);
		
	MiniPatch_CleanUp:
		safe_free(PatchMode);
		safe_free(CurrInput.ID);
		safe_free(ExprStr);
		safe_free(CurrVarStr);
	}

	return retval;
}

// Given a JSON row (which could come from a file or elsewhere) install the given patch.

BOOL Mod_InstallPatch(
	json_t *patchCurr,
	const char *path,
	const char *ModUUID,
	size_t i
){
	BOOL retval = TRUE;
	char *Mode = JSON_GetStr(patchCurr, "Mode");
	struct ModSpace input;

	memset(&input, 0, sizeof(struct ModSpace));
	CURRERROR = errNOERR;

	if(!Mode){
		AlertMsg("`Mode` not defined for patch.", "Mod Config Error");
		retval = FALSE;
		CURRERROR = errCRIT_MALLOC;
		goto Mod_InstallPatch_End;
	}
	
	// Change directory back to game's root in case it was changed.
	chdir(CONFIG.CURRDIR);
	ErrNo2ErrCode();
	if(CURRERROR != errNOERR){
		retval = FALSE;
		goto Mod_InstallPatch_End;
	}

	Mod_PatchKeyExists(patchCurr, "File", TRUE);
	
	// Fill input struct
	input = Mod_GetPatchInfo(patchCurr, path, ModUUID, i);
	if(input.Valid == FALSE){
		retval = FALSE;
		goto Mod_InstallPatch_End;
	}
	
	// Check variable condition
	if(Mod_PatchKeyExists(patchCurr, "Condition", FALSE)){
		char *CondEq = NULL;
		BOOL CanInstall;

		CondEq = JSON_GetStr(patchCurr, "Condition");
		CanInstall = Eq_Parse_Int(CondEq, ModUUID, FALSE);
		safe_free(CondEq);

		if(CURRERROR != errNOERR){
			retval = FALSE;
			goto Mod_InstallPatch_End;
		}

		if(!CanInstall){
			//AlertMsg("Skipping patch due to variable", "DEBUG");
			goto Mod_InstallPatch_End;
		}

	}

	/// Apply patches
	// Replace
	if(strieq(Mode, "Repl")){
		retval = ModOp_Clear(&input, ModUUID) &&
		ModOp_Add(&input, ModUUID);
	// Clear
	} else if(strieq(Mode, "Clear")){
		retval = ModOp_Clear(&input, ModUUID);
	// Add or Copy (Copy sets bytes beforehand to emulate Add)
	} else if(strieq(Mode, "Add") || strieq(Mode, "Copy")){
		retval = ModOp_Add(&input, ModUUID);
	// Reserve
	} else if(strieq(Mode, "Reserve")){
		retval = ModOp_Reserve(&input, ModUUID);
	// Move
	} else if(strieq(Mode, "Move")){
		// This is gonna get kinda weird.
		// Make a new modspace representing the source chunk
		struct ModSpace srcInput = input;
		srcInput.Start = srcInput.SrcStart;
		srcInput.End = srcInput.SrcEnd;
		
		// Remove the source
		retval = ModOp_Clear(&srcInput, ModUUID);
		if(retval == FALSE){
			goto Mod_InstallPatch_End;
		}
		
		// Add new one in new location
		retval = ModOp_Add(&input, ModUUID); //Add to Dest
	// Unknown mode
	} else {
		AlertMsg("Unknown patch mode", Mode);
		retval = FALSE;
		goto Mod_InstallPatch_End;
	}

	if(retval == FALSE){
		goto Mod_InstallPatch_End;
	}

	// Create vars for patch location
	{
		struct VarValue varCurr;
		char *FilePath;

		FilePath = File_GetPath(input.FileID);
		if(strndef(FilePath)){
			retval = FALSE;
			goto Mod_InstallPatch_End;
		}
		
		// Start
		varCurr.type = uInt32Pointer;
		asprintf(&varCurr.UUID, "Start.%s", input.PatchID); 
		varCurr.desc = strdup("Start byte of patch.");
		varCurr.publicType = strdup("");
		varCurr.mod = strdup(ModUUID);
		varCurr.persist = FALSE;
		varCurr.uInt32 = File_OffToPE(FilePath, input.Start);

		Var_MakeEntry(varCurr);
		Var_Destructor(&varCurr);

		// End
		varCurr.type = uInt32Pointer;
		asprintf(&varCurr.UUID, "End.%s", input.PatchID); 
		varCurr.desc = strdup("End byte of patch.");
		varCurr.publicType = strdup("");
		varCurr.mod = strdup(ModUUID);
		varCurr.persist = FALSE;
		varCurr.uInt32 = File_OffToPE(FilePath, input.Start);

		Var_MakeEntry(varCurr);
		Var_Destructor(&varCurr);

		safe_free(FilePath);
	}
	
	// Install "Mini Patches"
	{
		json_t *miniArray;
		
		miniArray = json_object_get(patchCurr, "MiniPatches");
		if(!miniArray || !json_is_array(miniArray)){
			goto Mod_InstallPatch_End;
		}
		
		if(!Mod_Install_Minipatch(miniArray, &input, path, ModUUID, i)){
			retval = FALSE;
		}
	}

Mod_InstallPatch_End:
    safe_free(Mode);
	safe_free(input.Bytes);
	safe_free(input.ID);
    safe_free(input.PatchID);
	return retval;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_Install
 *  Description:  Parses array of patches and adds/removes bytes depending on values.
 *                Patches support 6 operations, defined in manual.
 * =====================================================================================
 */
BOOL Mod_Install(json_t *root, const char *path)
{
	json_t *patchArray, *patchCurr;
	size_t i;
	char *ModUUID = JSON_GetStr(root, "UUID");
	BOOL retval = TRUE;
	
	//Define system-specific progress dialog
	ProgDialog_Handle ProgDialog;
	
	//Clear errors
	CURRERROR = errNOERR;
	
	//Init progress dialog
	patchArray = json_object_get(root, "patches");
	ProgDialog = ProgDialog_Init(
		json_array_size(patchArray),
		"Installing Mod..."
	);
	if(CURRERROR != errNOERR){
		ProgDialog_Kill(ProgDialog);
		return FALSE;
	}
	
	//Start SQL transaction (huge speedup!)
//	sqlite3_exec(CURRDB, "BEGIN TRANSACTION", NULL, 0, NULL);
	
	//Add variable entries
	{
		json_t *varArray, *varCurr;
		varArray = json_object_get(root, "variables");

		//Add user-defined variables
		json_array_foreach(varArray, i, varCurr){
			if(!Var_MakeEntry_JSON(varCurr, ModUUID)){
				retval = FALSE;
				goto Mod_Install_Cleanup;
			}
		}
		
		//Add modloader-defined variables
		{
			struct VarValue varCurr;
			int ModVer = JSON_GetuInt(root, "Version");
			
			//Active
			varCurr.type = uInt8;
			asprintf(&varCurr.UUID, "Active.%s", ModUUID); 
			varCurr.desc = strdup("Mod is installed.");
			varCurr.publicType = strdup("");
			varCurr.mod = strdup(ModUUID);
			varCurr.uInt32 = 1;
			varCurr.persist = FALSE;
			Var_MakeEntry(varCurr);
			
			//Version
			safe_free(varCurr.UUID);
			safe_free(varCurr.desc);
			varCurr.type = uInt32;
			varCurr.uInt32 = ModVer;
			asprintf(&varCurr.UUID, "Version.%s", ModUUID);
			varCurr.desc = strdup("Version of installed mod.");
			Var_MakeEntry(varCurr);
			
			//Cleanup
			Var_Destructor(&varCurr);
		}
	}
	
	//Install patches (if applicable)
	if(!patchArray || !json_is_array(patchArray)){
		goto Mod_Install_Cleanup;
	}
		
	json_array_foreach(patchArray, i, patchCurr){
		retval = Mod_InstallPatch(patchCurr, path, ModUUID, i);

		ProgDialog_Update(ProgDialog, 1);
		if(retval == FALSE || CURRERROR != errNOERR){
			char *msg = NULL;
			asprintf(&msg, "Mod configuration error on patch #%lu", i + 1);
			AlertMsg(msg, "JSON error");
			safe_free(msg);
			goto Mod_Install_Cleanup;
		}
	}
	
Mod_Install_Cleanup:
	safe_free(ModUUID);

	if(CURRERROR != errCRIT_MALLOC){
		Mod_AddToDB(root, path);
	}
	
	if(CURRERROR != errNOERR){retval = FALSE;}
	ProgDialog_Kill(ProgDialog);
	
//	sqlite3_exec(CURRDB, "END TRANSACTION", NULL, 0, NULL);
	return retval;
}

//Add mod to database
BOOL Mod_AddToDB(json_t *root, const char *path){
	sqlite3_stmt *command;
	char *uuid = JSON_GetStr(root, "UUID"); 
	char *name = JSON_GetStr(root, "Name");
	char *desc = JSON_GetStr(root, "Info");
	char *auth = JSON_GetStr(root, "Author");
	char *date = JSON_GetStr(root, "Date");
	char *cat = JSON_GetStr(root, "Category");
	int ver = JSON_GetuInt(root, "Version");
	
	const char *query = "INSERT INTO Mods "
		"('UUID', 'Name', 'Info', 'Author', 'Version', 'Date', 'Category', 'Path') VALUES "
		"(?, ?, ?, ?, ?, ?, ?, ?);";
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, uuid, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 2, name, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 3, desc, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 4, auth, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 5, ver) ||
		sqlite3_bind_text(command, 6, date, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 7, cat, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 8, path, -1, SQLITE_STATIC)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}
	command = NULL;
	
	safe_free(uuid);
	safe_free(name);
	safe_free(desc);
	safe_free(auth);
	safe_free(date);
	safe_free(cat);
	return TRUE;
}

BOOL Mod_FindDep(const char *ModUUID)
{
	int modCount;
	sqlite3_stmt *command;
	const char *query = "SELECT EXISTS(SELECT * FROM Dependencies "
		"JOIN Mods ON Dependencies.ParentUUID = Mods.UUID "
		"WHERE Dependencies.ChildUUID = ?)";
		
	CURRERROR = errNOERR;
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	
	modCount = SQL_GetNum(command);
	if(CURRERROR != errNOERR){return FALSE;}
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}
	
	return modCount != 0 ? TRUE : FALSE;
}

void Mod_FindDepAlert(const char *ModUUID){
	char *errormsg = NULL;
	sqlite3_stmt *command;
	unsigned int i, errorlen;
	json_t *out, *row;
	const char *query = "SELECT Name, Author, Version FROM Dependencies "
	                    "JOIN Mods ON Dependencies.ParentUUID = Mods.UUID "
	                    "WHERE Dependencies.ChildUUID = ?;";
	CURRERROR = errNOERR;
	
	// Get dependency list
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
	) != 0){CURRERROR = errCRIT_DBASE; return;}
	out = SQL_GetJSON(command);
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return;
	}
	command  = NULL;
	
	errormsg = strdup("Uninstalling this mod will require "
	                 "you to uninstall the following:\n\n");
	errorlen = strlen(errormsg);
	
	json_array_foreach(out, i, row){
		char *temp = NULL;
		int templen;
		char *name = JSON_GetStr(row, "Name");
		char *ver  = JSON_GetStr(row, "Version");
		char *auth = JSON_GetStr(row, "Author");

		//Print it into a string
		templen = asprintf(
			&temp, "- %s version %s by %s\n",
			name, ver, auth
		);
		
		while(strlen(errormsg) + templen + 1 >= errorlen){
			char *errormsgnew = NULL;
			errorlen *= errorlen;
			errormsgnew = realloc(errormsg, errorlen);
			if(errormsgnew == NULL){
				CURRERROR = errCRIT_MALLOC;
				safe_free(temp);
				json_decref(out);
				return;
			}
			errormsg = errormsgnew;
		}
		
		//Add it to the error message
		strncat(errormsg, temp, errorlen - strlen(errormsg));
		safe_free(temp);
		safe_free(name);
		safe_free(ver);
		safe_free(auth);
	}
	
	AlertMsg(errormsg, "Other Mods Preventing Uninstallation");
	safe_free(errormsg);
	json_decref(out);
	return;
}

// Remove entry for PatchUUID. Also un-claims previous space.
BOOL Mod_Uninstall_Remove(const char *PatchUUID)
{
	sqlite3_stmt *command;
	const char *query = "DELETE FROM Spaces WHERE ID = ? AND Version = ?;";
	CURRERROR = errNOERR;

	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 2, Mod_GetVerCount(PatchUUID))
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	command = NULL;
	
	Mod_UnClaimSpace(PatchUUID);
	return TRUE;
}

// Remove spaces created by split and restore split space to original state
BOOL ModOp_UnSplit(json_t *row)
{
	char *PatchUUID = JSON_GetStr(row, "ID");
	BOOL retval = FALSE;
	
	if(Mod_Uninstall_Remove(PatchUUID)){
		retval = TRUE;
	}
	
	safe_free(PatchUUID);
	return retval;
}

// Delete the file specified by File
BOOL ModOp_UnNew(json_t *row)
{
	int File = JSON_GetInt(row, "File");
	char *FilePath = File_GetPath(File);
	if(strndef(FilePath)){return FALSE;}

	File_Delete(FilePath); // DANGEROUS!

	safe_free(FilePath);

	// This just happens to work out with this implementation
	return ModOp_UnSplit(row);
}

// Write back the old bytes and then remove the space.
// Used for Add and Clear
BOOL ModOp_UnSpace(json_t *input, BOOL Revert)
{
	char *PatchUUID = JSON_GetStr(input, "PatchID");
	char *SpaceUUID = JSON_GetStr(input, "ID");
	CURRERROR = errNOERR;
	
	if(Revert){
		json_t *out, *row;
		sqlite3_stmt *command;
		const char *query1 = "SELECT OldBytes, Start FROM Revert WHERE PatchUUID = ? ";
		                     //"ORDER BY Version DESC LIMIT 1;";
		const char *query2 = "DELETE FROM Revert WHERE PatchUUID = ? ";
		                     /*"AND Version = ( "
		                         "SELECT MAX(Version) FROM Revert "
		                         "WHERE PatchUUID = ? "
		                     ");";*/

		int filehandle, offset, datalen;
		unsigned char *bytes;
		char *ByteStr;
		char *FileName = File_GetName(JSON_GetuInt(input, "File"));
		char *FilePath = NULL;

		// Open file handle
		if(FileName == NULL){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
		filehandle = File_OpenSafe(FilePath, _O_BINARY|_O_RDWR);

		if(filehandle == -1){
			return FALSE;
		} // Failure.

		// Retrieve data
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

		out = SQL_GetJSON(command);
		row = json_array_get(out, 0);
		if(!row){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

		ByteStr = JSON_GetStr(row, "OldBytes");
		offset = JSON_GetInt(row, "Start");
		json_decref(out);


		if(strndef(ByteStr)){
			return FALSE;
		}

		if(CURRERROR != errNOERR){
			safe_free(ByteStr);
			return FALSE;
		}
		
		if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
			safe_free(ByteStr);
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		command = NULL;

		// Convert data string from hex to raw bytes
		bytes = Hex2Bytes(ByteStr, &datalen);
		safe_free(ByteStr);
		
		// Restore the backup if it exists and the target file doesn't
		// (Are we still doing this?)
		{
			char *BakPath = NULL;
			asprintf(&BakPath, "%s/BACKUP/%s", CONFIG.CURRDIR, FileName);
			
			if(
				File_Exists(BakPath, FALSE, FALSE) &&
				!File_Exists(FilePath, FALSE, FALSE) &&
				CURRERROR == errWNG_READONLY
			){
				File_MovTree(BakPath, FilePath);
			}
			
			CURRERROR = errNOERR; //Reset error.
			safe_free(BakPath);
		}
		
		// Write back data
		File_WriteBytes(filehandle, offset, bytes, datalen);
		
		safe_free(bytes);
		safe_free(FilePath);
		safe_free(FileName);
		close(filehandle);
		
		// Remove the old bytes from the newest version
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		command = NULL;
	}
	
	// Remove the space
	Mod_Uninstall_Remove(SpaceUUID);
	
	// Unclaim any overlapping spaces
	{
		sqlite3_stmt *command;
		const char *query =
			"UPDATE Spaces SET UsedBy = NULL WHERE "
			"(End >= ? AND Start <= ?) AND File = ?";

		int start, end, file;
		start = JSON_GetuInt(input, "Start");
		end = JSON_GetuInt(input, "End");
		file = JSON_GetuInt(input, "File");
		
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_int(command, 1, start) ||
			sqlite3_bind_int(command, 2, end) ||
			sqlite3_bind_int(command, 3, file)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
	}

	safe_free(PatchUUID);
	safe_free(SpaceUUID);
	return TRUE;
}

// Uninstall a single space
BOOL Mod_Uninstall_Space(json_t *row, char **LastPatch)
{
	char *SpaceType = JSON_GetStr(row, "Type");
	char *SpacePatch = JSON_GetStr(row, "PatchID");
	BOOL Revert = FALSE;
	BOOL retval = TRUE;

	if(strndef(SpaceType)){
		// No more spaces
		retval = FALSE;
		goto Mod_Uninstall_Space_Cleanup;
	}
	
	// Perform space-specific remove operation
	if(
		strieq(SpaceType, "Add") ||
		strieq(SpaceType, "Clear")
	){

		// Set Revert flag
		if(strndef(*LastPatch) || strneq(*LastPatch, SpacePatch)){
			Revert = TRUE;
			safe_free(*LastPatch);
			*LastPatch = strdup(SpacePatch);
		}

		if(!ModOp_UnSpace(row, Revert)){
			retval = FALSE;
			goto Mod_Uninstall_Space_Cleanup;
		}

	} else if (
		strieq(SpaceType, "New")
	){
		if(!ModOp_UnNew(row)){
			retval = FALSE;
			goto Mod_Uninstall_Space_Cleanup;
		}

	} else if (
		strieq(SpaceType, "Split")
	){
		if(!ModOp_UnSplit(row)){
			retval = FALSE;
			goto Mod_Uninstall_Space_Cleanup;
		}
	}


Mod_Uninstall_Space_Cleanup:
	safe_free(SpaceType);
	safe_free(SpacePatch);

	return retval;
}

// Uninstall a single mod
BOOL Mod_Uninstall(const char *ModUUID)
{
	BOOL retval = TRUE;
	size_t i;
	json_t *out, *row;
	sqlite3_stmt *command;
	const char *query = "SELECT * FROM Spaces WHERE Mod = ? "
	                    "ORDER BY ROWID DESC";
	char *LastPatch = NULL;
	
	// Define progress dialog (handle type is interface-specific)
	ProgDialog_Handle ProgDialog;
	
	CURRERROR = errNOERR;
	
	chdir(CONFIG.CURRDIR); // In case path was changed during installation or w/e.
	
	//Check for dependencies
	if(Mod_FindDep(ModUUID) == TRUE){
		Mod_FindDepAlert(ModUUID);
		return FALSE;
	};

	//Get the spaces created by the mod
	if (SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
	) != 0) {
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	out = SQL_GetJSON(command);
	sqlite3_finalize(command);

	// Init progress box
	ProgDialog = ProgDialog_Init(json_array_size(out), "Uninstalling Mod...");
	
	// Parse rows
	json_array_foreach(out, i, row){

		if(!Mod_Uninstall_Space(row, &LastPatch)){
			goto Mod_Uninstall_Cleanup;
		}
		ProgDialog_Update(ProgDialog, 1);
	}

	json_decref(out);

	if(retval == FALSE){
		goto Mod_Uninstall_Cleanup;
	}
	
	// Mod has now been uninstalled.
	// Unmark used spaces
	{
		sqlite3_stmt *command;
		//Clear UsedBy if equal to UUID of current mod
		const char *query = "UPDATE Spaces SET UsedBy = NULL WHERE UsedBy = ?;";
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			retval = FALSE;
			goto Mod_Uninstall_Cleanup;
		}
		command = NULL;
	}
	
	// Remove dependencies
	{
		sqlite3_stmt *command;
		const char *query = "DELETE FROM Dependencies WHERE ParentUUID = ?;";
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			retval = FALSE;
			goto Mod_Uninstall_Cleanup;
		}
		command = NULL;
	}
	
	
	// Remove mod entry
	{
		sqlite3_stmt *command;
		const char *query = "DELETE FROM Mods WHERE UUID = ?;";
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			retval = FALSE;
			goto Mod_Uninstall_Cleanup;
		}
		command = NULL;
	}
	
	// Remove mod variables
	Var_ClearEntry(ModUUID);
	
Mod_Uninstall_Cleanup:
	// Kill progress box
	ProgDialog_Kill(ProgDialog);
	return retval;
}

// Uninstall everything below the given UUID
// Returns double-null-terminated list of the path of
// every uninstalled mod except for the input.
char * Mod_UninstallSeries(const char *UUID)
{
	unsigned int modCount, i, modStop;
	char *retList, *retPtr;
	unsigned int retListSize = 0, retListCap = 8;
/*	#ifdef _WIN32
		HWND ProgDialog;
	#endif*/

	// Count mods
	{
		sqlite3_stmt *command;
		const char *query = "SELECT COUNT(*) FROM Mods;";
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
		modCount = SQL_GetNum(command);
		if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
	}
	
	// Get stopping point (Uninstall everything up to selected mod)
	{
		sqlite3_stmt *command;
		const char *query = "SELECT RowID FROM Mods WHERE UUID = ?;";
		
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC)
		) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
		
		modStop = SQL_GetNum(command);
		if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
	}
	
	// Allocate list
	retList = malloc(retListCap);
	retPtr = retList;
	if (!retList) {
		CURRERROR = errCRIT_MALLOC;
		return NULL;
	}

	// Set up progress bar
	//ProgDialog = ProgDialog_Init(modCount - modStop, "Uninstalling...") ;
	
	// Loop through all mods
	for(i = modCount; i >= modStop; i--){
		char *CurrUUID = NULL;
		char *CurrPath = NULL;
		sqlite3_stmt *command;
		const char *query1 = "SELECT UUID FROM Mods WHERE RowID = ? LIMIT 1;";
		const char *query2 = "SELECT Path FROM Mods WHERE UUID = ? LIMIT 1";
		
		// Get UUID of current mod
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_int(command, 1, i)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			//Oh no.
			safe_free(retList);
			//retList = strdup("");
			//return retList;
			return NULL;
		}
		// Get UUID
		CurrUUID = SQL_GetStr(command);
		// Finalize
		sqlite3_finalize(command);
		command = NULL;

		// Get path
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, CurrUUID, -1, SQLITE_STATIC)
		) != 0 ){
			CURRERROR = errCRIT_DBASE;
			//Oh no. This is worse.
			safe_free(retList);
			safe_free(CurrUUID);
			//retList = strdup("");
			//return retList;
			return NULL;
		}

		CurrPath = SQL_GetStr(command);
		sqlite3_finalize(command);
		command = NULL;

		if (!CurrPath) {
			CURRERROR = errCRIT_MALLOC;
			return NULL;
		}

		// Uninstall!
		Mod_Uninstall(CurrUUID);
		
		//Record in list
		if(i == modStop){
			//Don't record first one.
			safe_free(CurrUUID);
			safe_free(CurrPath);
			continue;
		}
		while(retListSize + strlen(CurrPath) + 1 > retListCap){
			char *retListNew;
			retListCap *= 2;
			retListNew = realloc(retList, retListCap);
			if (!retListNew) {
				CURRERROR = errCRIT_MALLOC;
				return NULL;
			}
			retList = retListNew;
			retPtr = retList + retListSize;
		}
		strncpy(retPtr, CurrPath, retListCap - strlen(retPtr));
		retListSize += strlen(CurrPath) + 1;

		safe_free(CurrUUID);
		safe_free(CurrPath);
//		ProgDialog_Update(ProgDialog, 1);
	}
	
	// Throw on final null terminator
	// retListSize += 1;
	while(retListSize + 1 > retListCap){
		char *newRetList = NULL;
		retListCap *= 2;
		newRetList = realloc(retList, retListCap);

		if (newRetList == NULL) {
			CURRERROR = errCRIT_MALLOC;
			return NULL;
		}
		retList = newRetList;
	}
	retPtr = retList + retListSize;
	*retPtr = '\0';

//	ProgDialog_Kill(ProgDialog);

	return retList;
}

// Install every mod in the given double-null-terminated list in order
BOOL Mod_InstallSeries(const char *ModList)
{
	const char *modPath = ModList;
	BOOL retval = TRUE;
	
	while(*modPath != '\0' && retval == TRUE){
		//For each string...
		char *jsonPath = NULL;
		json_t *root;
		
		//Retreive JSON
		asprintf(&jsonPath, "%s/%s", modPath, "info.json");
		root = JSON_Load(jsonPath);
		
		//Install mod
		retval = Mod_Install(root, modPath);

		//Increment pointer
		if(retval == TRUE){
			modPath += strlen(modPath) + 1;
		}
		safe_free(jsonPath);
		json_decref(root);
	}
	
	//DO NOT rollback SQL. If you do, files on disk will
	//be in a strange half-written state. Instead, commit and then
	//immediately uninstall.
	if(retval == FALSE){
		Mod_Uninstall(modPath);
	}
	return TRUE;
}

//Uninstall every mod up to and including ModUUID, then reinstall in order
BOOL Mod_Reinstall(const char *ModUUID)
{
	char *retList = NULL;
	retList = Mod_UninstallSeries(ModUUID);
	
	//Install ModUUID
	{
		char *modDir = Mod_MangleUUID(ModUUID);
		char *modPath = NULL;
		char *jsonPath = NULL;
		json_t *root;
		
		//Retreive JSON
		asprintf(&modPath, "%s/mods/%s", CONFIG.CURRDIR, modDir);
		asprintf(&jsonPath, "%s/%s", modPath, "info.json");
		root = JSON_Load(jsonPath);
		
		//Install mod
		Mod_Install(root, modPath);
		
		//Deallocate crap
		safe_free(modPath);
		safe_free(modDir);
		safe_free(jsonPath);
		json_decref(root);
	}
	
	//Install rest
	Mod_InstallSeries(retList);
	safe_free(retList);
	return TRUE;
}
