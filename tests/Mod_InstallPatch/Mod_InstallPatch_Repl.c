#include "../../includes.h"
#include "../../funcproto.h"

int Test_Mod_InstallPatch_Repl()
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
    
    // Get function result
    result = Mod_InstallPatch(patch, modpath, "repl@test", 0);
    
    // Unload
    safe_free(modpath);
    json_decref(mod);
    
    // Test if function succeeded
    if(result == FALSE){
        fprintf(stderr, "Function Mod_InstallPatch returned FALSE.\n");
    }
    
    // Test if checksum is OK
    result &= Proto_Checksum("Mod_Bin", "repl.bin", 0xFFFFFFFF);
    
    // Test if Start.(whatever) variable exists
    
    return result;
}
