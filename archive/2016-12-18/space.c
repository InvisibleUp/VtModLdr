/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "includes.h"              // LOCAL: All includes
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages


//Return TRUE if the given patch UUID is installed, FALSE otherwise
BOOL Mod_SpaceExists(const char *PatchUUID)
{
	sqlite3_stmt *command;
	const char *query = "SELECT EXISTS(SELECT * FROM Spaces WHERE ID = ?)";
	int result;
	CURRERROR = errNOERR;
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	result = SQL_GetNum(command);
	
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return FALSE;
	}
	
	return (result > 0);
}

//Returns a ModSpace struct that matches the given space
struct ModSpace Mod_GetSpace(const char *PatchUUID)
{
	struct ModSpace result = {0};
	json_t *out, *row;
	sqlite3_stmt *command;
	const char *query = "SELECT * FROM Spaces WHERE ID = ? ORDER BY Ver DESC"; 
	
	CURRERROR = errNOERR;
	result.Valid = FALSE;
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return result;
	}
	
	out = SQL_GetJSON(command);
	
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return result;
	}
		
	//Make sure json output is valid
	if(!json_is_array(out)){
		CURRERROR = errCRIT_DBASE;
		return result;
	}
	//Make sure there is a space
	row = json_array_get(out, 0);
	if (!json_is_object(row)){
		return result;	//No error; no space
	}
	
	//Pull the data
	result.FileID = JSON_GetuInt(row, "File");
	result.Start = JSON_GetuInt(row, "Start");
	result.End = JSON_GetuInt(row, "End");
	result.ID = JSON_GetStr(row, "ID");
	
	result.Bytes = NULL;
	result.Len = result.End - result.Start;
	result.Valid = TRUE;

	json_decref(out);
	return result;
}

//Returns the type of the given space
char * Mod_GetSpaceType(const char *PatchUUID)
{
	sqlite3_stmt *command;
	const char *query = "SELECT Type FROM Spaces WHERE ID = ? ORDER BY Ver DESC"; 
	char *out = NULL;
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return strdup("");
	}
	
	out = SQL_GetStr(command);
	
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return strdup("");
	}
	
	return out;
}

//Renames a space in the SQL
BOOL Mod_RenameSpace(const char *OldID, const char *NewID)
{
	//We're purposely NOT using SQL_HandleErrors in case the caller
	//doesn't check if the new ID is already used.
	sqlite3_stmt *command;
	const char *query1 = "UPDATE Spaces SET ID = REPLACE(ID, ?, ?)";
	const char *query2 = "UPDATE Spaces SET Start = REPLACE(Start, ?, ?)";
	const char *query3 = "UPDATE Spaces SET End = REPLACE(End, ?, ?)";
	int SQLResult;
	
	SQLResult = sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL);
	
	sqlite3_bind_text(command, 1, OldID, -1, SQLITE_STATIC);
	sqlite3_bind_text(command, 2, NewID, -1, SQLITE_STATIC);
	
	SQLResult = sqlite3_step(command);
	sqlite3_finalize(command);
	
	if(!(
		SQLResult == SQLITE_OK ||
		SQLResult == SQLITE_DONE ||
		SQLResult == SQLITE_ROW
	)){
		return FALSE;
	}
		
	//If a MERGE or SPLIT space reference the old space, change ref
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, OldID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 2, NewID, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query3, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, OldID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 2, NewID, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	return TRUE;
}

// Mod_MakeBranchName
// Returns an "~x" name variant for new branch, with x being lowest possible

// Normally, if you try to get the ~x" variant of an "~x" variant, they
// will stack, leaving an ~x~0". After enough of these (only a couple
// dozen of them are needed), the program slows to an absolute crawl
// due to manipulating gigantic strings thousands of bytes in length.
// This tries to prevent this by checking if any branches have been
// made at all, and if so, increments ~x.

// These were formerly known as "old names". Those were really complex
// and computationally expensive, due to the fact they needed to be edited
// retroactively to every patch that used them. (Pretty much imagine the
// existing version system except attached to the name and in reverse.)
// Instead I added the version field. Much better.

char * Mod_MakeBranchName(const char *PatchUUID)
{
	int oldCount = 1;
	char *result = NULL;
	int PatchUUIDLen = strlen(PatchUUID);
	char *BaseOldPtr;
	
	//Check for branch chaining
	//Strip off everything after last ~ in PatchUUID
	BaseOldPtr = strrchr(PatchUUID, '~');
	if(BaseOldPtr){
		char *BaseName = NULL;
		char *BaseOld = NULL;

		BaseName = strdup(PatchUUID);
		BaseName[BaseOldPtr - PatchUUID] = '\0';

		//See if any branches have been made
		asprintf(&BaseOld, "%s~1", BaseName);
		
		//if(strcmp(BaseOld, PatchUUID) == 0){
		if(Mod_SpaceExists(BaseOld)){
			//Branch chain! That's bad.
			result = Mod_MakeBranchName(BaseName);
			safe_free(BaseName);
			safe_free(BaseOld);
			return result;
		}

		safe_free(BaseName);
		safe_free(BaseOld);
	}

	while(TRUE){
		//Actually do thing thing this function was intended to do
		asprintf(&result, "%s~%d", PatchUUID, oldCount);

		//Does patch name exist already?
		if(Mod_SpaceExists(result)){
			//Increment oldCount
			//(We never directly use it's value, so it's okay.)
			oldCount += 1;
			safe_free(result);
		} else {
			break;
		}
	}
	return result;
}

//Claims a space in the name of a mod by setting the UsedBy thing
BOOL Mod_ClaimSpace(const char *PatchUUID, const char *ModUUID)
{
	sqlite3_stmt *command;
	const char *query = "UPDATE Spaces SET UsedBy = ? WHERE ID = ? "
	                    "AND Ver = ?;";
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 2, PatchUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 3, Mod_GetVerCount(PatchUUID))
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	return TRUE;
}

// Unclaims the given space by claiming it in the name of nothing.
// It's lazy, but it works.
BOOL Mod_UnClaimSpace(const char *PatchUUID)
{
	return Mod_ClaimSpace(PatchUUID, "");
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_FindSpace
 *  Description:  Finds free space in the file for modification.
 *                The given chunk of space is the smallest that
 *                is within the bounds given by the input.
 * =====================================================================================
 */
struct ModSpace Mod_FindSpace(const struct ModSpace *input, BOOL IsClear)
{
	struct ModSpace result = {0};
	json_t *out, *row;
	
	//Set defaults for result
	CURRERROR = errNOERR;
	result.Valid = FALSE;

	//Do the finding
	{
		sqlite3_stmt *command;
		const char *query = "SELECT * FROM Spaces WHERE "
		                    "Type = ? AND (UsedBy IS NULL OR "
							"UsedBy == '') AND "
		                    "Len >= ? AND Start <= ? AND End >= ? "
		                    "AND File = ? ORDER BY Len;";

		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1,
				IsClear ? "Clear":"Add", -1, SQLITE_STATIC) ||
			sqlite3_bind_int(command, 2, input->Len) ||
			sqlite3_bind_int(command, 3, input->Start) ||
			sqlite3_bind_int(command, 4, input->End) ||
			sqlite3_bind_int(command, 5, input->FileID)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return result;
		}
		
		out = SQL_GetJSON(command);
		
		if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE;
			return result;
		}
	}
	
	//Make sure json output is valid
	if(!json_is_array(out)){
		CURRERROR = errCRIT_DBASE;
		return result;
	}

	if (json_array_size(out) == 0) {
		json_decref(out);
		return result;
	}
	
	row = json_array_get(out, 0);
	
	//Pull data
	result.FileID = JSON_GetuInt(row, "File");
	result.Start = JSON_GetuInt(row, "Start");
	result.End = JSON_GetuInt(row, "End");
	result.ID = JSON_GetStr(row, "ID");
	
	result.Bytes = NULL;
	result.Len = result.End - result.Start;
	result.Valid = TRUE;

	json_decref(out);
	return result;
}

struct ModSpace Mod_FindParentSpace(const struct ModSpace *input)
{
	//Note: Branches are non-issue. Branches fork off; they don't
	//replace the original.
	sqlite3_stmt *command;
	const char *query = 
		"SELECT * FROM Spaces WHERE (UsedBy IS NULL "
		"OR UsedBy == '') AND File = ? AND Start <= ? AND End >= ?";
	json_t *out, *row;

	struct ModSpace result = {0};
	CURRERROR = errNOERR;

	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_int(command, 1, input->FileID) ||
		sqlite3_bind_int(command, 2, input->Start) ||
		sqlite3_bind_int(command, 3, input->End) //|
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return result;
	}
	
	out = SQL_GetJSON(command);
	
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; 
		return result;
	}
		
	//Make sure json output is valid
	if(!json_is_array(out)){
		CURRERROR = errCRIT_DBASE;
		return result;
	}
	//Make sure there is a space
	row = json_array_get(out, 0);
	if (!json_is_object(row)){
		return result;	//No error; no space
	}
	
	//Pull the data
	result.FileID = JSON_GetuInt(row, "File");
	result.Start = JSON_GetuInt(row, "Start");
	result.End = JSON_GetuInt(row, "End");
	result.ID = JSON_GetStr(row, "ID");
	
	result.Bytes = NULL;
	result.Len = result.End - result.Start;
	result.Valid = TRUE;

	json_decref(out);
	return result;
}

//Return the value of the greatest version for PatchUUID
int Mod_GetVerCount(const char *PatchUUID)
{
	sqlite3_stmt *command;
	const char *query = "SELECT COUNT(*) FROM Spaces WHERE ID = ?";
	int verCount;
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return -1;
	}
	
	verCount = SQL_GetNum(command);
	sqlite3_finalize(command);
	if(verCount == -1 && CURRERROR == errNOERR){
		verCount = 0;
	}
	
	return verCount;
}

char * Mod_FindPatchOwner(const char *PatchUUID)
{
	char *out = NULL;
	
	sqlite3_stmt *command;
	const char *query = "SELECT ID FROM Spaces "
	                    "JOIN Mods ON Spaces.Mod = Mods.UUID "
	                    "WHERE Spaces.ID = ? ORDER BY Spaces.Ver DESC";
	CURRERROR = errNOERR;
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return strdup("");
	}
	
	out = SQL_GetStr(command);
	
	if(CURRERROR != errNOERR){
		safe_free(out);
		return strdup("");
	}
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		safe_free(out);
		return strdup("");
	}
	return out;
}

BOOL Mod_SplitSpace(
	struct ModSpace *out,
	const char *HeadID,
	const char *TailID,
	const char *OldID,
	const char *ModUUID,
	int splitOff,
	BOOL retHead
){
	char *OldPatchType = NULL;
	char *OldIDNew = NULL;
	struct ModSpace OldPatch = {0};
	BOOL retval = FALSE;
	
	sqlite3_stmt *command;
	const char *query = "INSERT INTO Spaces "
		"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len', 'UsedBy', 'Ver') VALUES "
		"(?, ?, ?, ?, ?, ?, ?, ?, ?);";

	CURRERROR = errNOERR;
	
	//Sanity assertions
	//Oh. What do we do when we're splitting different versions?
	//(This WILL happen.)
	/*if(streq(HeadID, TailID)){
		AlertMsg("HeadID == TailID", "DEBUG");
		CURRERROR = errCRIT_ARGMNT;
		return FALSE;
	}*/
		
	//Get patch data for OldID
	OldPatch = Mod_GetSpace(OldID);
	OldPatchType = Mod_GetSpaceType(OldID);

	//More sanity assertions
	if(OldPatch.Valid == FALSE){
		CURRERROR = errCRIT_ARGMNT;
		return FALSE;
	}
	
	if(streq(HeadID, OldID) || streq(TailID, OldID)){
		//We should have a unique ID for this internal bookkeeping junk
		asprintf(&OldIDNew, "%s-%s", HeadID, TailID);
	} else {
		OldIDNew = strdup(OldID);
	}
	
	//Claim the original space in our name
	Mod_ClaimSpace(OldID, "MODLOADER@invisbleup");
	
	//Prepare DB
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_SplitSpace_Return;
	}
	
	//Create HeadID
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, HeadID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 2, OldPatchType, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 3, OldPatch.FileID) ||
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 5, OldPatch.Start) ||
		sqlite3_bind_int(command, 6, OldPatch.Start + splitOff) ||
		sqlite3_bind_int(command, 7, splitOff) ||
//		sqlite3_bind_text(command, 8, ModUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_null(command, 8) ||
		sqlite3_bind_int(command, 9, Mod_GetVerCount(HeadID) + 1)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_reset(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_SplitSpace_Return_PostDB;
	}
	
	//Create TailID
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, TailID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 2, OldPatchType, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 3, OldPatch.FileID) ||
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 5, OldPatch.Start + splitOff) ||
		sqlite3_bind_int(command, 6, OldPatch.End) ||
		sqlite3_bind_int(command, 7, OldPatch.Len - splitOff) ||
//		sqlite3_bind_text(command, 8, ModUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_null(command, 8) ||
		sqlite3_bind_int(command, 9, Mod_GetVerCount(TailID) + 1)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_reset(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_SplitSpace_Return_PostDB;
	}
	
	//Create PatchUUID
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, OldIDNew, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 2, "Split", -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 3, OldPatch.FileID) ||
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 5, HeadID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 6, TailID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 7, OldPatch.Len) ||
		sqlite3_bind_text(command, 8, ModUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 9, Mod_GetVerCount(OldIDNew) + 1)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_reset(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_SplitSpace_Return_PostDB;
	}
	
	//Write back positional data
	if(out && retHead){
		out->Start = OldPatch.Start;
		out->End = OldPatch.Start + splitOff;

		if(out->ID != HeadID && out->ID != TailID){
			safe_free(out->ID);
		}
		out->ID = strdup(HeadID);

	} else if (out && !retHead) {
		out->Start = OldPatch.Start + splitOff + 1;
		out->End = OldPatch.End;

		if(out->ID != HeadID && out->ID != TailID){
			safe_free(out->ID);
		}
		out->ID = strdup(TailID);
	}
	
	retval = TRUE;
	
Mod_SplitSpace_Return_PostDB:
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		retval = FALSE;
	}
Mod_SplitSpace_Return:
	safe_free(OldIDNew);
	safe_free(OldPatch.Bytes);
	safe_free(OldPatch.ID);
	safe_free(OldPatchType);
	
	return retval;
}

// Merge the given two spaces into a larger space
// Currently broken and, frankly, useless.
/*BOOL Mod_MergeSpace(const struct ModSpace *input, const char *HeadID, const char *TailID, const char *ModUUID)
{
	struct ModSpace HeadPatch = {0};
	struct ModSpace TailPatch = {0};
	char *HeadType = NULL;
	char *TailType = NULL;
	
	char *AutoID = NULL;
	char *NewID = NULL;
	char *NewIDOld = NULL;
	char *HeadIDNew = NULL;
	char *TailIDNew = NULL;
	
	sqlite3_stmt *command;
	const char *query = "INSERT INTO Spaces "
		"('ID', 'Ver', 'Type', 'File', 'Mod', 'Start', 'End', 'Len', 'UsedBy') VALUES "
		"(?, ?, ?, ?, ?, ?, ?, ?, ?);";
	
	BOOL retval = FALSE;
	
	//Get Head and Tail information
	HeadPatch = Mod_GetSpace(HeadID);
	HeadType = Mod_GetSpaceType(HeadID);
	TailPatch = Mod_GetSpace(TailID);
	TailType = Mod_GetSpaceType(TailID);
	
	//Bail if we need to
	if(strcmp(HeadType, TailType) == 0){
		//Types not equal. Unsupported action.
		goto Mod_MergeSpace_Return;
	}
	
	if(
		strcmp(HeadType, "Merge") == 0 ||
		strcmp(HeadType, "Split") == 0
	){
		//One of the spaces is MERGE or SPLIT. This is unsupported.
		goto Mod_MergeSpace_Return;
	}
	
	//Create a new ID if we need to
	asprintf(&AutoID, "%d-%X", input->FileID, input->Start);
	if(
		strcmp(input->ID, "") == 0 || //No ID
		strcmp(input->ID, AutoID) == 0 //Auto-generated ID

	){
		asprintf(&NewID, "%s-%s", HeadID, TailID);
	} else {
		NewID = strdup(input->ID);
	}
	safe_free(AutoID);

	//Mark Head and Tail as used.
	Mod_ClaimSpace(HeadID, ModUUID);
	Mod_ClaimSpace(TailID, ModUUID);
	
	//Bang out some SQL (Merge space, then [type] space)
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_MergeSpace_Return;
	}

	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, NewID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 2, 0) || // Version
		sqlite3_bind_text(command, 3, "Merge", -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 4, HeadPatch.FileID) ||
		sqlite3_bind_text(command, 5, ModUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 6, HeadID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 7, TailID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 8, 0) ||
		sqlite3_bind_text(command, 9, ModUUID, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_reset(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_MergeSpace_Return;
	}
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, NewID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 2, 1) || // Version
		sqlite3_bind_text(command, 3, HeadType, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 4, HeadPatch.FileID) ||
		sqlite3_bind_text(command, 5, ModUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 6, HeadPatch.Start) ||
		sqlite3_bind_int(command, 7, TailPatch.End) ||
		sqlite3_bind_int(command, 8, TailPatch.End - HeadPatch.Start) ||
		sqlite3_bind_text(command,98, ModUUID, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_reset(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_MergeSpace_Return;
	}
	
	sqlite3_finalize(command);
	
	retval = TRUE;
	
	
Mod_MergeSpace_Return:
	safe_free(NewID);
	safe_free(HeadPatch.ID)
	safe_free(HeadType);
	safe_free(TailPatch.ID);
	safe_free(TailType);
	
	return retval;
}*/

// Splice one space into another space using SplitSpace.
// More complicated than you'd think.
BOOL Mod_SpliceSpace(
	struct ModSpace *parent,
	struct ModSpace *child,
	const char *ModUUID
){
	char *parentName = strdup(parent->ID);
	char *childName = strdup(child->ID);
	char *PatchName = parentName;
	BOOL wasEqual = FALSE;

	//Things break horribly if IDs are the same thing
	if(parent->ID == child->ID){
		safe_free(childName);
		childName = strdup(parentName);
		wasEqual = TRUE;
	}
	
	//Create head section
	if(child->Start != parent->Start){
		Mod_SplitSpace(
			parent, //ModSpace
			parentName, //Head
			childName, //Tail
			parentName,//Patch ID
			ModUUID,
			child->Start - parent->Start, //SplitOff
			FALSE //Return head?
		);
		
		if (wasEqual) {
			//Would have been freed by SplitSpace, leaving Child with an invalid ID
			child->ID = parent->ID;
		}
		if (CURRERROR != errNOERR){return FALSE;}

		// Splitting messes up the names, so we have to adjust.
		PatchName = childName;
		parentName = Mod_MakeBranchName(parentName); // Leaks memory
		child->End += 1; //I dunno. Makes it work, though.
	}
	
	//Create tail section
	if(child->End != parent->End){
		Mod_SplitSpace(
			parent, //ModSpace
			childName, //Head
			parentName, //Tail
			PatchName, //Patch ID
			//child->ID,
			ModUUID,
			child->End - parent->Start, //SplitOff
			TRUE //Return head?
		);

		if (wasEqual) {
			//Would have been freed by SplitSpace, leaving Child with an invalid ID
			child->ID = parent->ID;
		}
		if (CURRERROR != errNOERR) { return FALSE; }
	}

	safe_free(parentName);
	safe_free(childName);
	PatchName = NULL;

	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Mod_MakeSpace
 *  Description:  Creates an occupied space marker in the given space.
 *                No sanity checking is done whatsoever.
 * =====================================================================================
 */
BOOL Mod_MakeSpace(struct ModSpace *input, const char *ModUUID, BOOL Clear){
	//Compute the wanted Start address and End address
	//Tries to go as low as possible, but sometimes the ranges clash.
	sqlite3_stmt *command;
	const char *query = "INSERT INTO Spaces "
		"('ID', 'Type', 'File', 'Mod', 'Start', 'End', 'Len', 'Ver') VALUES "
		"(?, ?, ?, ?, ?, ?, ?, ?);";
	char *FilePath = NULL, *FileName = NULL;
	int PEStart, PEEnd, Ver;

	CURRERROR = errNOERR;

	//Convert input.Start and input.End to PE format if needed
	FileName = File_GetName(input->FileID);
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);

	//Convert to PE offset
	if(File_IsPE(FilePath)){
		PEStart = File_OffToPE(FilePath, input->Start);
		PEEnd = File_OffToPE(FilePath, input->End);
	} else {
		PEStart = input->Start;
		PEEnd = input->End;
	}

	safe_free(FileName);
	safe_free(FilePath);
	
	//Get value of VER
	Ver = Mod_GetVerCount(input->ID) + 1;
	if(Ver == -1){return FALSE;}

	//If replacing something, mark that space as used	
	if(Ver > 0){
		Mod_ClaimSpace(input->ID, ModUUID);
	}
	
	//Create new AddSpc table row
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, input->ID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 2, Clear ? "Clear" : "Add", -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 3, input->FileID) ||
		sqlite3_bind_text(command, 4, ModUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 5, PEStart) ||
		sqlite3_bind_int(command, 6, PEEnd) ||
		sqlite3_bind_int(command, 7, PEEnd - PEStart) ||
		sqlite3_bind_int(command, 8, Ver) 
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	command = NULL;
	return TRUE;
}

//Read existing bytes into revert table and write new bytes in
BOOL Mod_CreateRevertEntry(const struct ModSpace *input)
{
	unsigned char *OldBytesRaw = NULL;
	char *OldBytes = NULL;
	char *FileName = NULL, *FilePath = NULL;
	int handle = -1;
	sqlite3_stmt *command;
	const char *query = "INSERT INTO Revert "
		"('PatchUUID', 'Ver', 'OldBytes') VALUES (?, ?, ?);";
	BOOL retval = FALSE;
	
	//If using virtual memory file, exit.
	if(input->FileID == 0){
		retval = TRUE;
		goto Mod_CreateRevertEntry_Return;
	}
	
	//Read the old bytes
	OldBytesRaw = calloc(input->Len, sizeof(char));
	//Quick error checking
	if(OldBytesRaw == NULL){
		CURRERROR = errCRIT_MALLOC;
		goto Mod_CreateRevertEntry_Return;
	}
	
	//Get file path from input struct
	FileName = File_GetName(input->FileID);
	asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
	
	//Open handle
	handle = File_OpenSafe(FilePath, _O_BINARY|_O_RDWR);
	if(handle == -1){
		goto Mod_CreateRevertEntry_Return;
	}

	//Actually read the file. (I actually forgot this.)
	lseek(handle, input->Start, SEEK_SET);
	read(handle, OldBytesRaw, input->Len);

	close(handle);
	handle = -1;
	
	//Convert to hex string
	OldBytes = Bytes2Hex(OldBytesRaw, input->Len);
	safe_free(OldBytesRaw);
	
	//Construct & Execute SQL Statement
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, input->ID, -1, SQLITE_STATIC) ||
		sqlite3_bind_int(command, 2, Mod_GetVerCount(input->ID)) ||
		sqlite3_bind_text(command, 3, OldBytes, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		goto Mod_CreateRevertEntry_Return;
	}
	command = NULL;
	retval = TRUE;
	
	
Mod_CreateRevertEntry_Return:
	safe_free(OldBytes);
	safe_free(FileName);
	safe_free(FilePath);
	
	return retval;
}

//Best way to do this is to go ahead and produce a mod with Mov operations
//that can be uninstalled like any other mod.
//(MOVE won't work. But we'd probably want to mark something when we merge.)
//(Add MERGE/SPLIT spaces to SQL? Might be needed.)

///NEW
//Search for where Space.End = DifferentSpace.Start
	//Delete DifferentSpace
//Loop through results
	//Set Space.End to DifferentSpace.End
	//Recalculate Len
//Free n' shizz

///OLD
/*For every file id:
	struct ModSpace[] resultArray =
	SELECT * FROM ClearSpc WHERE
	'FileID' = [fileid]
	ORDER BY 'Start';
	
	Look at next ClearSpcID.
	See if first END is more than next START.
	If so, combine the two*/
	
// Too computationally expensive and likely to screw up.
// I'll deal with this later.
// Or, considering how things have gone, never.

/*BOOL Mod_MergeFreeSpace()
{
	// Just for the record, this query is extremely slow.
	// (Takes at least 2.5 seconds to run for me.)
	// Use sparingly.
	const char *query = 
		"SELECT a.ID AS A, b.ID AS B FROM Spaces a "
		"JOIN Spaces b "
		"WHERE a.Type = 'Clear' AND b.Type = 'Clear' AND a.ID != b.ID "
		"AND (a.Start = b.End OR a.Start = b.End+1) "
		"AND a.Ver = (SELECT MAX(Ver) FROM Spaces WHERE Spaces.ID = a.ID) "
		"AND b.Ver = (SELECT MAX(Ver) FROM Spaces WHERE Spaces.ID = b.ID) ";
	sqlite3_stmt *command;
	json_t *out, *row;
	size_t i;
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 ){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	out = SQL_GetJSON(command);
	sqlite3_finalize(command);
	
	json_array_foreach(out, i, row){
		//Merge A and B (no-repeat guarantee)
		Mod_MergeSpace(
	}
	
	return TRUE;
	
}*/

// Delete any files are are filled with only Clear spaces
// Because who needs those?
// (Currently unused. Fix that.)
BOOL Mod_RemoveAllClear(){
	// Query could use EXISTS, but I can't figure out how to do that.
	// Probably should, because this takes about 0.4s to run.
	const char *query =
		"SELECT File, COUNT(*) AS Count FROM Spaces"
		"WHERE File != 0 AND Type != 'Clear' "
		"AND (UsedBy = '' OR UsedBy IS NULL) "
		"GROUP BY File ";
	sqlite3_stmt *command;
	json_t *out, *row;
	size_t i;
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 ){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	out = SQL_GetJSON(command);
	sqlite3_finalize(command);
	
	json_array_foreach(out, i, row){
		//Get file path
		char *FileName = NULL;
		char *FilePath = NULL;
		char *BakPath = NULL;
		int FileID;
		
		FileID = JSON_GetuInt(row, "File");
		FileName = File_GetName(FileID);
		asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
		asprintf(&BakPath, "%s/BACKUP/%s", CONFIG.CURRDIR, FileName);
		
		// Move the file to a backup directory
		File_MovTree(FilePath, BakPath);
		
		//(This will get restored when the restorer writes to it)
		safe_free(FileName);
		safe_free(FilePath);
		safe_free(BakPath);
	}
	json_decref(out);
	return TRUE;	
}