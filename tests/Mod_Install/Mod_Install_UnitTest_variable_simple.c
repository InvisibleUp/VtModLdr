#include "../../includes.h"
#include "../../funcproto.h"

int Test_Mod_Install_UnitTest_variable_simple()
{
	json_t *mod;
    char *modpath;
    BOOL result = TRUE;
    
    // Load first patch
    asprintf(&modpath, "%s/test/Mod_/variable_simple.json", CONFIG.PROGDIR);
    mod = JSON_Load(modpath);

    result = Mod_Install(mod, modpath);
    
    // Unload
    safe_free(modpath);
    json_decref(mod);
    
    // Test if function succeeded
    if(result == FALSE){
        fprintf(stderr, "Function Mod_Install returned FALSE.\n");
        return FALSE;
    }
    
    // Test if checksum is unchanged
    if(!Proto_Checksum("test.bin", 0xE20EEA22, TRUE)){
        fprintf(stderr, "Checksum error on test.bin\n");
        return FALSE;
    }
    
    // Check if new variable exists and is expected value
    if(!Var_Exists("var.variable_simple@test")){
        fprintf(stderr, "Variable var.variable_simple@test not found\n");
        return FALSE;
    }
    
    {
        struct VarValue var = Var_GetValue_SQL("var.variable_simple@test");
        if(var.uInt8 != 42){
            fprintf(stderr, "Value does not match! (Found %d, expected %d)\n", var.uInt8, 42);
            return FALSE;
        }
        Var_Destructor(&var);
    }
    
    return TRUE;
}
