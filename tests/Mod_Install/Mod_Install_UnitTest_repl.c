#include "../../includes.h"
#include "../../funcproto.h"

int Test_Mod_Install_UnitTest_repl()
{
	json_t *mod;
    char *modpath;
    BOOL result = TRUE;
    
    // Load first patch
    asprintf(&modpath, "%s/test/Mod_/repl.json", CONFIG.PROGDIR);
    mod = JSON_Load(modpath);

    result = Mod_Install(mod, modpath);
    
    // Unload
    safe_free(modpath);
    json_decref(mod);
    
    // Test if function succeeded
    if(result == FALSE){
        fprintf(stderr, "Function Mod_Install returned FALSE.\n");
        return result;
    }
    
    // Test if checksum is OK
    result = Proto_Checksum("test.bin", 0xFFFFFFFF, TRUE);

    return result;
}
