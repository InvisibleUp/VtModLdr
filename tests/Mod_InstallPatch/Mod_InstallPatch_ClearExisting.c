// Tests if we can sucessfully install ClearExisting

#include "../../includes.h"
#include "../../funcproto.h"

int Test_Mod_InstallPatch_ClearExisting()
{
	json_t *mod, *patcharr, *patch;
    char *modpath;
    BOOL result = TRUE;
    
    // Load first patch
    asprintf(&modpath, "%s/test/Mod_/clear_existing.json", CONFIG.PROGDIR);
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
    result = Mod_InstallPatch(patch, modpath, "clear_existing@test", 0);
    
    // Unload
    safe_free(modpath);
    json_decref(mod);
    
    // Test if function succeeded
    if(result == FALSE){
        fprintf(stderr, "Function Mod_InstallPatch returned FALSE.\n");
        return result;
    }
    
    // Test if checksum is OK
    result = Proto_Checksum("test.bin", 0x89AE2423, TRUE);
    
    return result;
}
