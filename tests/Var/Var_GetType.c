// Tests if Var_GetType function works
#include "../../includes.h"
#include "../../funcproto.h"

int Test_Var_GetType()
{
    enum VarType out;
    
    // Check if type mapping works properly
    out = Var_GetType("uInt8");
    if(out != uInt8){
        fprintf(stderr, "Expected uInt8 (%d), got %d\n", uInt8, out);
        return FALSE;
    }
    
    // Check if type mapping works improperly
    out = Var_GetType("definitely invalid");
    if(out != INVALID){
        fprintf(stderr, "Expected INVALID (%d), got %d\n", INVALID, out);
        return FALSE;
    }
    
    return TRUE;
} 
