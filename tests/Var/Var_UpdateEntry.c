// Tests if Var_GetValue_SQL function works
#include "../../includes.h"
#include "../../funcproto.h"

int Test_Var_UpdateEntry()
{
    struct VarValue var;
    
    // Create variable
    if(!Test_Var_MakeEntry()){
        fprintf(stderr, "Test Var_MakeEntry needs to pass for this to pass.");
        return FALSE;
    }
    
    var = Var_GetValue_SQL("ID.test@invisibleup");
    if(var.type == INVALID){
        fprintf(stderr, "Var_GetValue_SQL failed.");
        return FALSE;
    }
    
    var.uInt8 = 128;
    
    if(!Var_UpdateEntry(var)){
        fprintf(stderr, "Var_UpdateEntry failed.");
        return FALSE;
    }
    
    // Ensure value has actually been updated
    Var_Destructor(&var);
    
    var = Var_GetValue_SQL("ID.test@invisibleup");
    if(var.uInt8 != 128){
        fprintf(stderr, "Value does not match! (Found %d, expected %d)", var.uInt8, 128);
        return FALSE;
    }
    Var_Destructor(&var);
    
    return TRUE;
    
}
