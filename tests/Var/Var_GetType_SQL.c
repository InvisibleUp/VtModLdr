// Tests if Var_GetType_SQL function works
#include "../../includes.h"
#include "../../funcproto.h"

int Test_Var_GetType_SQL()
{
    enum VarType out;
    
    // Create variable
    if(!Test_Var_MakeEntry()){
        fprintf(stderr, "Test Var_MakeEntry needs to pass for this to pass.");
        return FALSE;
    }
    
    // Check if type mapping works properly
    out = Var_GetType_SQL("ID.test@invisibleup");
    if(out != uInt8){
        fprintf(stderr, "Expected uInt8 (%d), got %d\n", uInt8, out);
        return FALSE;
    }
    
    return TRUE;
} 
 
