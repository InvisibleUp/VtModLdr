// Tests if Var_Exists function works
#include "../../includes.h"
#include "../../funcproto.h"

int Test_Var_Exists()
{
    // Create variable
    if(!Test_Var_MakeEntry()){
        fprintf(stderr, "Test Var_MakeEntry needs to pass for this to pass.");
        return FALSE;
    }
    
    // Check if Var_Exists return true, because the var exists...
    return Var_Exists("ID.test@invisibleup");
}
