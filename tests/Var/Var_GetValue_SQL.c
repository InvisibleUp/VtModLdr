// Tests if Var_GetValue_SQL function works
#include "../../includes.h"
#include "../../funcproto.h"

int Test_Var_GetValue_SQL()
{
    struct VarValue var;
    
    // Create variable
    if(!Test_Var_MakeEntry()){
        fprintf(stderr, "Test Var_MakeEntry needs to pass for this to pass.");
        return FALSE;
    }
    
    // Check if Var_Exists return true, because the var exists...
    var = Var_GetValue_SQL("ID.test@invisibleup");
    
    {
        const char *desc = "Description";
        const char *mod = "test@invisibleup";
        const char *type = "uInt8";
        const char val = 42;
        
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
            fprintf(stderr, "Value does not match! (Found %d, expected %d)", var.uInt8, val);
            return FALSE;
        }
    }
    
    return TRUE;
} 
