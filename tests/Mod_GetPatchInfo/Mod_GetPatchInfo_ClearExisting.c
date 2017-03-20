// Tests if we can get s valid start and end from just a patch name

#include "../../includes.h"
#include "../../funcproto.h"

int Test_Mod_GetPatchInfo_ClearExisting()
{
	json_t *mod, *patcharr, *patch;
    char *modpath;
    struct ModSpace out;
    const char BytesExpected[] = {0xFF, 0xFF, 0xFF, 0xFF};
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
    out = Mod_GetPatchInfo(patch, modpath, "clear_existing@test", 0);
    
    // Unload
    safe_free(modpath);
    json_decref(mod);
    
    // Test
    if(out.Valid != TRUE){
        fprintf(
            stderr, 
            "out.Valid has incorrect value\n"
            "\t(expected %d, got %d).\n", 
            TRUE, out.Valid
        );
        result = FALSE;
    }
    if(out.Start != 131072){
        fprintf(
            stderr, 
            "out.Start has incorrect value\n"
            "\t(expected %d, got %d).\n", 
            131072, out.Start
        );
        result = FALSE;
    }
    if(out.End != 262144){
        fprintf(
            stderr, 
            "out.End has incorrect value\n"
            "\t(expected %d, got %d).\n", 
            262144, out.End
        );
        result = FALSE;
    }
    if(out.Len != 131072){
        fprintf(
            stderr, 
            "out.Len has incorrect value\n"
            "\t(expected %d, got %d).\n", 
            131072, out.Len
        );
        result = FALSE;
    }
    // The rest is all internal stuff that is up to change
    // and can be tested individually
    
    return result;
}
