// Tests if we can sucessfully create a variable entry
// and it exists in the SQL

#include "../../includes.h"
#include "../../funcproto.h"

int Test_Var_MakeEntry()
{
    json_t *out, *row;
    sqlite3_stmt *command;
    const char *query = "SELECT * FROM Variables WHERE UUID = 'ID.test@invisibleup'";
    
    struct VarValue var = {0};
    var.desc = "Description";
    var.UUID = "ID.test@invisibleup";
    var.mod = "test@invisibleup";
    var.type = uInt8;
    var.uInt8 = 42;
    
    if(!Var_MakeEntry(var)){
        return FALSE;
    }
    
    // Make sure it exists in SQL
    
    if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return FALSE;
	}
	
	out = SQL_GetJSON(command);
	
    //Make sure json output is valid
	if(
        SQL_HandleErrors(
            __FILE__, __LINE__, sqlite3_finalize(command)
        ) != 0 || !json_is_array(out) 
    ){
		CURRERROR = errCRIT_DBASE; 
		return FALSE;
	}
		
	//Make sure there is a result
	row = json_array_get(out, 0);
	if (!json_is_object(row)){
		return FALSE;	//No error; no space
	}
	
    // Verify row contents
    {
        char *desc, *mod, *type;
        int val;
        
        desc = JSON_GetStr(row, "Desc");
        mod = JSON_GetStr(row, "Mod");
        type = JSON_GetStr(row, "Type");
        val = JSON_GetInt(row, "Value");
        
        if(strneq(desc, var.desc)){
            fprintf(stderr, "Desc does not match! (Found %s, expected %s)", desc, var.desc);
            return FALSE;
        }
        if(strneq(mod, var.mod)){
            fprintf(stderr, "Mod does not match! (Found %s, expected %s)", mod, var.mod);
            return FALSE;
        }
        if(strneq(type, "uInt8")){
            fprintf(stderr, "Type does not match! (Found %s, expected uInt8)", type);
            return FALSE;
        }
        if(val != var.uInt8){
            fprintf(stderr, "Value does not match! (Found %d, expected %d)", var.uInt8, 42);
            return FALSE;
        }
        
        safe_free(desc);
        safe_free(mod);
        safe_free(type);
    }
    
    return TRUE;
    
}
