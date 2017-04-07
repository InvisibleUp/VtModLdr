#include "../../includes.h"
#include "../../funcproto.h"

int Test_Mod_Install_UnitTest_clear_existing()
{
	json_t *mod;
    char *modpath;
    BOOL result = TRUE;
    
    // Load first patch
    asprintf(&modpath, "%s/test/Mod_/clear_existing.json", CONFIG.PROGDIR);
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
    result = Proto_Checksum("test.bin", 0x89AE2423, TRUE);
    
    // Test if Start.(whatever) variable exists
    
    return result;
}
