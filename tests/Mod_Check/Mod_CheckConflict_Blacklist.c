#include "../../includes.h"
#include "../../funcproto.h"

int Test_Mod_CheckConflict_Blacklist()
{
	json_t *mod;
    char *modpath;
    BOOL result = FALSE;
    
    asprintf(&modpath, "%s/test/Mod_/blacklisted.json", CONFIG.PROGDIR);
    
    mod = JSON_Load(modpath);
    safe_free(modpath);
    if(!mod){
        fprintf(stderr, "JSON_Load error [1]");
    }
    
    result = !Mod_CheckConflict(mod);
    
    json_decref(mod);
    return result;
}

