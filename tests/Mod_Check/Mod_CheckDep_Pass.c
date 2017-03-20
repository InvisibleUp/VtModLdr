#include "../../includes.h"
#include "../../funcproto.h"

int Test_Mod_CheckDep_Pass()
{
	json_t *mod;
    char *modpath;
    BOOL result = FALSE;
    
    asprintf(&modpath, "%s/test/Mod_/repl.json", CONFIG.PROGDIR);
    
    mod = JSON_Load(modpath);
    safe_free(modpath);
    if(!mod){
        fprintf(stderr, "JSON_Load error [1]");
    }
    
    result = Mod_CheckDep(mod);
    
    json_decref(mod);
    return result;
}

