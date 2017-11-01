#include "../../includes.h"
#include "../../funcproto.h"

int Test_Mod_Install_UnitTest_VarRepatch()
{
	json_t *mod;
    char *modpath, *modfile;
    BOOL result = TRUE;
    
    // Load first mod
    asprintf(&modpath, "%s/test/Mod_/variable_repatch/host/", CONFIG.PROGDIR);
    asprintf(&modfile, "%sinfo.json", modpath);
    mod = JSON_Load(modfile);
    if(!mod){
        safe_free(modfile);
        safe_free(modpath);
        return FALSE;
    }

    result = Mod_Install(mod, modpath);

    safe_free(modfile);
    safe_free(modpath);
    json_decref(mod);
    
    if(result == FALSE){
        fprintf(stderr, "Host mod installation returned FALSE.\n");
        return FALSE;
    }
    
    // Load second mod
    asprintf(&modpath, "%s/test/Mod_/variable_repatch/user/", CONFIG.PROGDIR);
    asprintf(&modfile, "%sinfo.json", modpath);
    mod = JSON_Load(modfile);
    if(!mod){
        safe_free(modfile);
        safe_free(modpath);
        return FALSE;
    }

    result = Mod_Install(mod, modpath);

    safe_free(modfile);
    safe_free(modpath);
    json_decref(mod);
    
    if(result == FALSE){
        fprintf(stderr, "User mod installation returned FALSE.\n");
        return FALSE;
    }
    
    // Check if mods are installed
    if(!Var_Exists("Active.variable_repatch_host@test")){
        fprintf(stderr, "Variable Active.variable_repatch_host@test not found\n");
        return FALSE;
    }
    
    if(!Var_Exists("Active.variable_repatch_user@test")){
        fprintf(stderr, "Variable Active.variable_repatch_user@test not found\n");
        return FALSE;
    }
    
    // Check if new variable exists and is expected value
    if(!Var_Exists("End.ClearSpot.variable_repatch_host@test")){
        fprintf(stderr, "Variable End.ClearSpot.variable_repatch_host@test not found\n");
        return FALSE;
    }
    
    {
        struct VarValue var = Var_GetValue_SQL("End.ClearSpot.variable_repatch_host@test");
        if(var.uInt8 != 4){
            fprintf(stderr, "Value for End.ClearSpot.variable_repatch_host@test does not match! (Found %d, expected %d)\n", var.uInt8, 4);
            return FALSE;
        }
        Var_Destructor(&var);
    }
    
    return TRUE;
}
