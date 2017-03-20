/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "includes.h"              // LOCAL: All includes
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages

// Returns true if the given variable is saved in the SQL.
BOOL Var_Exists(const char *VarUUID)
{
	int result;
	sqlite3_stmt *command;
	const char *query = 
		"SELECT EXISTS ( SELECT * FROM Variables WHERE UUID = ? )";
	CURRERROR = errNOERR;
	
	//Get SQL results and put into VarObj
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, VarUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	result = SQL_GetNum(command);
	
	if(CURRERROR != errNOERR || SQL_HandleErrors(
		__FILE__, __LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	return (result != 0);
}

//Effectively this function is a poor-man's string lookup table.
enum VarType Var_GetType(const char *type)
{
	enum VarType result;
	if(streq(type, "IEEE32")){
		result = IEEE32;
	} else if (streq(type, "IEEE64")){
		result = IEEE64;
	} else if (streq(type, "Int8")){
		result = Int8;
	} else if (streq(type, "Int16")){
		result = Int16;
	} else if(streq(type, "Int32")){
		result = Int32;
	} else if (streq(type, "uInt8")){
		result = uInt8;
	} else if (streq(type, "uInt16")){
		result = uInt16;
	} else if (streq(type, "uInt32")){
		result = uInt32;
	} else if (streq(type, "uInt32Pointer")){
		result = uInt32Pointer;
	} else {
		result = INVALID;
	}
	return result;
}

//Ditto, except this takes a VarType enum and spits out a textual representation.
const char * Var_GetType_Str(enum VarType type)
{
	switch(type){
	case IEEE64: return "IEEE64";
	case IEEE32: return "IEEE32";
	case Int8:   return "Int8";
	case Int16:  return "Int16";
	case Int32:  return "Int32";
	case uInt8:  return "uInt8";
	case uInt16: return "uInt16";
	case uInt32: return "uInt32";
	case uInt32Pointer: return "uInt32Pointer"; // This was pretty...
	default:     return "INVALID";
	}
}

// Ditto, except it fetches directly from the SQL
enum VarType Var_GetType_SQL(const char *VarUUID){
	struct VarValue CurrVar;
	enum VarType result;

	CurrVar = Var_GetValue_SQL(VarUUID);
	result = CurrVar.type;
	Var_Destructor(&CurrVar);

	return result;
}

//Fetches bytes from stored SQL table
struct VarValue Var_GetValue_SQL(const char *VarUUID){
	struct VarValue result;
	json_t *VarArr, *VarObj;
	sqlite3_stmt *command;
	char *query = "SELECT * FROM Variables WHERE UUID = ? LIMIT 1";
	CURRERROR = errNOERR;
	
	//Get SQL results and put into VarObj
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, VarUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		result.type = INVALID;
		return result;
	}
	
	VarArr = SQL_GetJSON(command);

	if(CURRERROR != errNOERR || SQL_HandleErrors(
		__FILE__, __LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		result.type = INVALID;
		return result;
	}
	
	VarObj = json_array_get(VarArr, 0);
	
	//Get type
	result.type = Var_GetType(JSON_GetStr(VarObj, "Type"));
	
	//Get value
	switch(result.type){
	case IEEE64:
		result.IEEE64 = (double)JSON_GetDouble(VarObj, "Value"); break;
	case IEEE32:
		result.IEEE32 = (float)JSON_GetDouble(VarObj, "Value"); break;
	case Int32:
		result.Int32  = (int32_t)JSON_GetInt(VarObj, "Value"); break;
	case Int16:
		result.Int16  = (int16_t)JSON_GetInt(VarObj, "Value"); break;
	case Int8:
		result.Int8   = (int8_t)JSON_GetInt(VarObj, "Value"); break;
	case uInt32:
	case uInt32Pointer:
		result.uInt32 = (uint32_t)JSON_GetuInt(VarObj, "Value"); break;
	case uInt16:
		result.uInt16 = (uint16_t)JSON_GetuInt(VarObj, "Value"); break;
	case uInt8:
		result.uInt8  = (uint8_t)JSON_GetuInt(VarObj, "Value"); break;	
    default:
        result.uInt32 = 0;
	}
	
	//Get UUID, desc, publicType
	result.UUID = JSON_GetStr(VarObj, "UUID");
	result.desc = JSON_GetStr(VarObj, "Info");
	result.publicType = JSON_GetStr(VarObj, "PublicType");
	result.mod = JSON_GetStr(VarObj, "Mod");
	
	json_decref(VarArr);
	return result;
}

// Converts JSON to variable value
// Now just a cast around the given expression
struct VarValue Var_GetValue_JSON(json_t *VarObj, const char *ModUUID)
{
	struct VarValue var;
	char *eq = NULL;
	char *VarID = NULL;
	char *type = NULL;
	enum VarType newtype = INVALID;
	CURRERROR = errNOERR;

	// Get old value
	VarID = JSON_GetStr(VarObj, "UUID");
	var = Var_GetValue_SQL(VarID);
	
	// Get type
	type = JSON_GetStr(VarObj, "Type");
	newtype = Var_GetType(type);
	safe_free(type);

	// Check if type is same if var already exists
	if(var.type != INVALID && var.type != newtype){
		CURRERROR = errWNG_MODCFG;
		var.type = INVALID;
		safe_free(VarID);
		return var;
	}

	// Load "default" or "update" expression, depending on what's needed.
	if(var.type == INVALID){
		eq = JSON_GetStr(VarObj, "Default");

		// Deallocate non-existent var and set base value to 0
		Var_Destructor(&var);
		switch(newtype){
		case IEEE64: var.IEEE64 = 0; break;
		case IEEE32: var.IEEE32 = 0; break;
		default: var.uInt32 = 0; break;
		}

	} else {
		eq = JSON_GetStr(VarObj, "Update");
		if (strndef(eq)) {
			// Update must not be defined. Try "Default".
			eq = JSON_GetStr(VarObj, "Default");
		}
	}

	// Finally set type
	var.type = newtype;

	// Handle remaining errors now
	if(strndef(eq) || var.type == INVALID){
		CURRERROR = errWNG_MODCFG;
		var.type = INVALID;
		safe_free(VarID);
		return var;
	}

	// Parse and typecast expression
	if(var.type == IEEE64 || var.type == IEEE32){
		double value = Eq_Parse_Double(eq, ModUUID, FALSE);

		switch (var.type){
		default:
		case IEEE32:
			var.IEEE32 += (float)value;
		case IEEE64:
			var.IEEE64 += (double)value;
		}
	} else if (var.type == Int32 || var.type == Int16 || var.type == Int8){
		int32_t value = Eq_Parse_Int(eq, ModUUID, FALSE);

		switch(var.type){
		default:
		case Int32:
			var.Int32  += (int32_t)value; break;
		case Int16:
			var.Int16  += (int16_t)value; break;
		case Int8:
			var.Int8   += (int8_t)value; break;
		}
	} else {
		uint32_t value = Eq_Parse_uInt(eq, ModUUID, FALSE);

		switch(var.type){
		default:
		case uInt32Pointer:
		case uInt32:
			var.uInt32 += (uint32_t)value; break;
		case uInt16:
			var.uInt16 += (uint16_t)value; break;
		case uInt8:
			var.uInt8  += (uint8_t)value; break;	
		}
	}
	
	//Get UUID, desc, publicType
	var.UUID = VarID;
	var.desc = JSON_GetStr(VarObj, "Info");
	var.publicType = JSON_GetStr(VarObj, "PublicType");
	var.mod = strdup(ModUUID);
	var.persist = JSON_GetuInt(VarObj, "Persist") ? TRUE : FALSE;
	
	return var;
}

//Get length, in bytes, of the given variable
int Var_GetLen(const struct VarValue *var)
{
	switch(var->type){
	case IEEE64:
		return 8;
	break;
		
	case Int32:
	case uInt32:
	case IEEE32:
	case uInt32Pointer:
		return 4;
	break;
	
	case Int16:
	case uInt16:
		return 2;
	break;
	
	case Int8:
	case uInt8:
		return 1;
	break;
	
	default:
		return 0;
	}
}

// Returns an integer value for a given variable
int Var_GetInt(const struct VarValue *var)
{
	switch(var->type){
	case IEEE64:
		return (int)var->IEEE64;
	case IEEE32:
		return (int)var->IEEE32;
	case Int32:
		return (int)var->Int32;
	case Int16:
		return (int)var->Int16;
	case Int8:
		return (int)var->Int8;
	case uInt32:
	case uInt32Pointer:
		return (int)var->uInt32;
	case uInt16:
		return (int)var->uInt16;
	case uInt8:
		return (int)var->uInt8;
	default:
		return 0;
	}
}

// Returns a double value for a given variable
double Var_GetDouble(const struct VarValue *var)
{
	switch(var->type){
	case IEEE64:
		return (double)var->IEEE64;
	case IEEE32:
		return (double)var->IEEE32;
	case Int32:
		return (double)var->Int32;
	case Int16:
		return (double)var->Int16;
	case Int8:
		return (double)var->Int8;
	case uInt32:
	case uInt32Pointer:
		return (double)var->uInt32;
	case uInt16:
		return (double)var->uInt16;
	case uInt8:
		return (double)var->uInt8;
	default:
		return 0;
	}
}

// Prepares a VarValue to leave scope
void Var_Destructor(struct VarValue *var)
{
	safe_free(var->UUID);
	safe_free(var->desc);
	safe_free(var->publicType);
	safe_free(var->mod);
	var = NULL;
}

// Compare variable to integer/float/raw input according to string Mode.
// inlen is maximum size of variables (in case of raw data)
// Soon to be replaced (or used heavily by) with expression parser!
/*BOOL Var_Compare(
	const struct VarValue *var, 
	const char *mode, 
	const void *input,
	const int inlen
){
	int varlen, memcmpRes;
	enum CompType{EQ, NOTEQ, LT, GT, LTE, GTE} comptype;
	
	//Get length of variable datatype
	varlen = Var_GetLen(var);
	if(varlen == 0){return FALSE;}
	
	varlen = min(varlen, inlen);
	
	//Store compare operation as enum
	if(strieq(mode, "EQUALS")){
		comptype = EQ;
	} else if (strieq(mode, "NOT EQUALS")){
		comptype = NOTEQ;
	} else if (strieq(mode, "LESS")){
		comptype = LT;
	} else if (strieq(mode, "GREATER")){
		comptype = GT;
	} else if (strieq(mode, "LESS EQUAL")){
		comptype = LTE;
	} else if (strieq(mode, "GREATER EQUAL")){
		comptype = GTE;
	} else {
		return FALSE;
	}
	
	//If float, do standard comparison
	if(var->type == IEEE32){
		float CondVal = *(float*)input;
		switch(comptype){
		case EQ:
			return (var->IEEE32 == CondVal);
		case NOTEQ:
			return (var->IEEE32 != CondVal);
		case LT:
			return (var->IEEE32 < CondVal);
		case GT:
			return (var->IEEE32 > CondVal);
		case LTE:
			return (var->IEEE32 <= CondVal);
		case GTE:
			return (var->IEEE32 >= CondVal);
		default:
			return FALSE;
		}
	} else if (var->type == IEEE64){
		double CondVal = *(double*)input;
		switch(comptype){
		case EQ:
			return (var->IEEE64 == CondVal);
		case NOTEQ:
			return (var->IEEE64 != CondVal);
		case LT:
			return (var->IEEE64 < CondVal);
		case GT:
			return (var->IEEE64 > CondVal);
		case LTE:
			return (var->IEEE64 <= CondVal);
		case GTE:
			return (var->IEEE64 >= CondVal);
		default:
			return FALSE;
		}
	}
	
	//Do memcmp comparisons
	memcmpRes = memcmp(var->raw, input, varlen);
	
	switch(comptype){
		case EQ:
			return (memcmpRes == 0);
		case NOTEQ:
			return (memcmpRes != 0);
		case LT:
			return (memcmpRes < 0);
		case GT:
			return (memcmpRes > 0);
		case LTE:
			return (memcmpRes <= 0);
		case GTE:
			return (memcmpRes >= 0);
		default:
			return FALSE;
	}
}*/

//Create Public Variable Listbox entires in SQL database
void Var_CreatePubList(json_t *PubList, const char *UUID)
{
	json_t *row;
	unsigned int i;
	CURRERROR = errNOERR;
	
	json_array_foreach(PubList, i, row){
		char *label = JSON_GetStr(row, "Label");
		int num = JSON_GetInt(row, "Value");
		sqlite3_stmt *command;
		const char *query = "INSERT INTO VarList (Var, Number, Label) "
		                    "VALUES (?, ?, ?);";
		
		//Get SQL results and put into VarObj
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, UUID, -1, SQLITE_STATIC) ||
			sqlite3_bind_int(command, 2, num) ||
			sqlite3_bind_text(command, 3, label, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
			safe_free(label);
			CURRERROR = errCRIT_DBASE;
			return;
		}
		command = NULL;
		safe_free(label);
	}	
}

//Remove a variable entry and it's listbox entry
BOOL Var_ClearEntry(const char *ModUUID)
{
	//Remove ListBoxes
	{
		sqlite3_stmt *command;
		const char *query = 
			"DELETE FROM VarList WHERE Var IN ("
				"SELECT Var FROM VarList "
				"JOIN Variables ON VarList.Var = Variables.UUID "
				"WHERE Variables.Persist = 0 AND "
				"Variables.Mod = ? "
			");";

		CURRERROR = errNOERR;
		
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

	}

	// Run Var_Unpatch on all variables
	Var_UnPatchMod(ModUUID);
	
	//Remove variables
	{
		sqlite3_stmt *command;
		const char *query = "DELETE FROM Variables WHERE Mod = ? AND Persist = 0";
		CURRERROR = errNOERR;
		
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
	}
	
	return TRUE;
}

//Update a variable
BOOL Var_UpdateEntry(struct VarValue result)
{
	//Update existing variable instead of making a new one.	
	sqlite3_stmt *command;
	const char *query = "UPDATE Variables SET "
			    "'Value' = ? WHERE UUID = ?;";
	CURRERROR = errNOERR;
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 2, result.UUID, -1, SQLITE_STATIC)
	) != 0){CURRERROR = errCRIT_DBASE; return FALSE;}
	   
	//Swap out Value for appropriate type.
	switch(result.type){
	case IEEE64:
		sqlite3_bind_double(command, 1, result.IEEE64); break;
	case IEEE32:
		sqlite3_bind_double(command, 1, result.IEEE32); break;
	case Int32:
		sqlite3_bind_int(command, 1, result.Int32); break;
	case Int16:
		sqlite3_bind_int(command, 1, result.Int16); break;
	case Int8:
		sqlite3_bind_int(command, 1, result.Int8); break;
	case uInt32:
	case uInt32Pointer:
		sqlite3_bind_int(command, 1, result.uInt32); break;
	case uInt16:
		sqlite3_bind_int(command, 1, result.uInt16); break;
	case uInt8:
		sqlite3_bind_int(command, 1, result.uInt8); break;
	default:
		return FALSE;
	}
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE; return FALSE;
	}
	command = NULL;

	// Reinstall effected patches
	Var_RePatch(result.UUID);

	return TRUE;
}

BOOL Var_RePatch(const char *VarUUID) {
	const char *query = "SELECT * FROM VarRepatch WHERE Var = ?";
	sqlite3_stmt *command;
	json_t *out, *row;
	size_t i;

	if (SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, VarUUID, -1, SQLITE_STATIC)
	) != 0) {
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	out = SQL_GetJSON(command);
	sqlite3_finalize(command);

	json_array_foreach(out, i, row) {

		json_t *mod, *patchArray, *patch;
		char *modUUID, *modPath, *jsonPath;
		int patchNo;

		// Load selected mod JSON
		modPath = JSON_GetStr(row, "ModPath");
		patchNo = JSON_GetInt(row, "PatchNo");
		asprintf(&jsonPath, "%sinfo.json", modPath);

		mod = JSON_Load(jsonPath);
		if (mod == NULL) {
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		safe_free(jsonPath);

		modUUID = JSON_GetStr(mod, "UUID");
		patchArray = json_object_get(mod, "patches");
		if (mod == NULL) {
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

		patch = json_array_get(patchArray, patchNo);
		if (patch == NULL) {
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}

		//AlertMsg("We're repatching!", "DEBUG");

		// Uninstall patch
		{
			size_t i;
			json_t *out, *row;
			sqlite3_stmt *command;
			struct ModSpace patchSpace;
			const char *query = "SELECT * FROM Spaces WHERE PatchID = ? "
								"ORDER BY ROWID DESC";
			char *LastPatch = NULL;

			// Get space
			patchSpace = Mod_GetPatchInfo(patch, modPath, modUUID, patchNo);

			//Get the spaces created by the mod
			if (SQL_HandleErrors(__FILE__, __LINE__, 
				sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
			) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
				sqlite3_bind_text(command, 1, patchSpace.PatchID, -1, SQLITE_STATIC)
			) != 0) {
				CURRERROR = errCRIT_DBASE;
				return FALSE;
			}

			out = SQL_GetJSON(command);
			sqlite3_finalize(command);
			
			// Parse rows
			json_array_foreach(out, i, row){
				Mod_Uninstall_Space(row, &LastPatch);
			}

			safe_free(patchSpace.Bytes);
			safe_free(patchSpace.ID);
			safe_free(patchSpace.PatchID);
			json_decref(out);
		}

		// Reinstall patch
		Mod_InstallPatch(patch, modPath, modUUID, patchNo);
		
	}

	return TRUE;
}

// Undoes all Var_RePatch operations on mod uninstall
BOOL Var_UnPatch(const char *VarUUID, const char *ModPath)
{
	sqlite3_stmt *command;
	char *query = "SELECT OldVal FROM VarRePatch WHERE VarUUID = ? AND ModPath = ? LIMIT 1;";
	struct VarValue result;
	json_t *VarArr, *VarObj;
	
	// Compose SQL
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, VarUUID, -1, SQLITE_STATIC) ||
		sqlite3_bind_text(command, 2, ModPath, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	VarArr = SQL_GetJSON(command);

	if(CURRERROR != errNOERR || SQL_HandleErrors(
		__FILE__, __LINE__, sqlite3_finalize(command)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	VarObj = json_array_get(VarArr, 0);

	//Get type
	result.type = Var_GetType(JSON_GetStr(VarObj, "Type"));
	
	//Get value
	switch(result.type){
	case IEEE64:
		result.IEEE64 = (double)JSON_GetDouble(VarObj, "Value"); break;
	case IEEE32:
		result.IEEE32 = (float)JSON_GetDouble(VarObj, "Value"); break;
	case Int32:
		result.Int32  = (int32_t)JSON_GetInt(VarObj, "Value"); break;
	case Int16:
		result.Int16  = (int16_t)JSON_GetInt(VarObj, "Value"); break;
	case Int8:
		result.Int8   = (int8_t)JSON_GetInt(VarObj, "Value"); break;
	case uInt32:
	case uInt32Pointer:
		result.uInt32 = (uint32_t)JSON_GetuInt(VarObj, "Value"); break;
	case uInt16:
		result.uInt16 = (uint16_t)JSON_GetuInt(VarObj, "Value"); break;
	case uInt8:
		result.uInt8  = (uint8_t)JSON_GetuInt(VarObj, "Value"); break;	
    default:
        result.uInt32 = 0;
	}

	// Update entry. Yeah, this'll make a RePatch, but we need it.
	Var_UpdateEntry(result);

	json_decref(VarArr);
	return TRUE;

}

// Runs Var_UnPatch for all variables updated my a single mod
BOOL Var_UnPatchMod(const char *ModUUID)
{
	char *ModPath = NULL;
	json_t *out, *row;
	sqlite3_stmt *command;
	char *query1 = "SELECT Path FROM Mods WHERE UUID = ?";
	char *query2 = "SELECT Var FROM VarRepatch WHERE ModPath = ?";
	size_t i;

	// Get path from mod name
	if (SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, ModUUID, -1, SQLITE_STATIC)
	) != 0 ){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}

	ModPath = SQL_GetStr(command);
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_finalize(command)
	) != 0) {
		CURRERROR = errCRIT_DBASE;
		safe_free(ModPath);
		return FALSE;
	}

	// Get list of all variables in VarRepatch table with ModPath
	if (SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, ModPath, -1, SQLITE_STATIC)
	) != 0 ){
		CURRERROR = errCRIT_DBASE;
		safe_free(ModPath);
		return FALSE;
	}

	out = SQL_GetJSON(command);
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_finalize(command)
	) != 0) {
		CURRERROR = errCRIT_DBASE;
		safe_free(ModPath);
		json_decref(out);
		return FALSE;
	}

	// Iterate through all variables
	json_array_foreach(out, i, row){
		char *VarUUID = JSON_GetStr(row, "Var");
		Var_UnPatch(VarUUID, ModPath);
		safe_free(VarUUID);
	}

	safe_free(ModPath);
	json_decref(out);
	return TRUE;

}

// Create a variable entry
BOOL Var_MakeEntry(struct VarValue result)
{
	CURRERROR = errNOERR;
	// Check if variable already exists
	if(Var_GetValue_SQL(result.UUID).type != INVALID){
		return Var_UpdateEntry(result);
	}	
		
	// Compose and execute an insert command.
	{	
		sqlite3_stmt *command;
		char *query = "INSERT INTO Variables "
		              "(UUID, Mod, Type, PublicType, Info, Value, Persist) "
			      "VALUES (?, ?, ?, ?, ?, ?, ?);";
		
		// Compose SQL
		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, result.UUID, -1, SQLITE_STATIC) ||
			sqlite3_bind_text(command, 2, result.mod, -1, SQLITE_STATIC) ||
			sqlite3_bind_text(command, 3, Var_GetType_Str(result.type), -1, SQLITE_STATIC) ||
			sqlite3_bind_text(command, 4, result.publicType, -1, SQLITE_STATIC) ||
			sqlite3_bind_text(command, 5, result.desc, -1, SQLITE_STATIC) ||
			sqlite3_bind_int(command, 7, result.persist)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
				
		// Swap out Value for appropriate type.
		switch(result.type){
		case IEEE64:
			sqlite3_bind_double(command, 6, result.IEEE64); break;
		case IEEE32:
			sqlite3_bind_double(command, 6, result.IEEE32); break;
		case Int32:
			sqlite3_bind_int(command, 6, result.Int32); break;
		case Int16:
			sqlite3_bind_int(command, 6, result.Int16); break;
		case Int8:
			sqlite3_bind_int(command, 6, result.Int8); break;
		case uInt32:
		case uInt32Pointer:
			sqlite3_bind_int(command, 6, result.uInt32); break;
		case uInt16:
			sqlite3_bind_int(command, 6, result.uInt16); break;
		case uInt8:
			sqlite3_bind_int(command, 6, result.uInt8); break;
		default:
			sqlite3_bind_int(command, 6, 0); break;
		}
		if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
			CURRERROR = errCRIT_DBASE; return FALSE;
		}
		command = NULL;
	}
		
	return TRUE;
}

// Create a variable entry from mod configuration JSON
BOOL Var_MakeEntry_JSON(json_t *VarObj, const char *ModUUID)
{
	struct VarValue result;
	BOOL retval = FALSE;
	CURRERROR = errNOERR;
	
	//Get variable value and type
	result = Var_GetValue_JSON(VarObj, ModUUID);
	if(CURRERROR != errNOERR){
		return FALSE;
	}
	
	//If PublicType == list, create and insert the PublicList
	//This may cause issues if you don't do this manually and call
	//Var_MakeEntry directly. Eh.
	if(streq(result.publicType, "List")){
		json_t *PubList = json_object_get(VarObj, "PublicList");
		Var_CreatePubList(PubList, result.UUID);
	}

	// Insert via SQL
	retval = Var_MakeEntry(result);
	
	//Free junk in those variables
	safe_free(result.UUID);
	safe_free(result.desc);
	safe_free(result.publicType);
	safe_free(result.mod);
	
	return retval;
}

// Dereferences a pointer if the variable type is a pointer
// Returns TRUE if successfuly dereferenced
BOOL Var_DerefPointer(struct VarValue *var){
	char *pch = var->UUID;
	int FileID;

	if(var->type != uInt32Pointer){
		return FALSE;
	}

	// Check for ".Start"
	if(strncmp(pch, "Start.", strlen("Start.")) != 0){
		return FALSE;
	}

	// Get patch name. For autogenerated names this is just the rest of
	// the string.
	pch += strlen("Start.");

	// Find file patch belongs to
	{
		sqlite3_stmt *command;
		char *query = "SELECT File FROM Spaces WHERE PatchID = ? LIMIT 1";

		if(SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
		) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
			sqlite3_bind_text(command, 1, pch, -1, SQLITE_STATIC)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
		
		FileID = SQL_GetNum(command);

		if(CURRERROR != errNOERR || SQL_HandleErrors(
			__FILE__, __LINE__, sqlite3_finalize(command)
		) != 0){
			CURRERROR = errCRIT_DBASE;
			return FALSE;
		}
	}

	// Get path from File ID
	{
		char *FilePath = File_GetPath(FileID);
		if(strndef(FilePath)){return FALSE;}

		var->uInt32 = File_PEToOff(FilePath, (uint32_t)var->uInt32);
		safe_free(FilePath);
	}

	return TRUE;
}
