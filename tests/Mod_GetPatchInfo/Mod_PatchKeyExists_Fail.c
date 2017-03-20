#include "../../includes.h"
#include "../../funcproto.h"

int Test_Mod_PatchKeyExists_Fail()
{
	json_t *mod, *patcharr, *patch;
    char *modpath;
    BOOL result = TRUE;
    
    // Load first patch
    asprintf(&modpath, "%s/test/Mod_/repl.json", CONFIG.PROGDIR);
    mod = JSON_Load(modpath);
    if(!mod){
        fprintf(stderr, "Could not load mod .json\n");
        return FALSE;
    }
    patcharr = json_object_get(mod, "patches");
    if(!patcharr){
        fprintf(stderr, "Could not find patches array\n");
        return FALSE;
    }
    patch = json_array_get(patcharr, 0);
    if(!patch){
        fprintf(stderr, "Could not get patch\n");
        return FALSE;
    }
    
    result = Mod_PatchKeyExists(patch, "Start", FALSE);
    
    json_decref(mod);
    return result;
}
