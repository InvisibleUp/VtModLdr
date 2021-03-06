// Autogenerated by genmapping_c.bat
#include "../includes.h"
#include "../funcproto.h"
#include <time.h>

int Test_Caller(const char *input){
    if(streq(input, "Test_Caller.c")){ 
         clock_t start = clock(); 
         int result = Test_Test_Caller(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Test_Caller", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Eq_Parse_Int_ConstDec.c")){ 
         clock_t start = clock(); 
         int result = Test_Eq_Parse_Int_ConstDec(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Eq_Parse_Int_ConstDec", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Eq_Parse_Int_ConstHex.c")){ 
         clock_t start = clock(); 
         int result = Test_Eq_Parse_Int_ConstHex(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Eq_Parse_Int_ConstHex", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "asprintf_Basic.c")){ 
         clock_t start = clock(); 
         int result = Test_asprintf_Basic(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "asprintf_Basic", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "CRC32_File_Basic.c")){ 
         clock_t start = clock(); 
         int result = Test_CRC32_File_Basic(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "CRC32_File_Basic", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "_InitSeq.c")){ 
         clock_t start = clock(); 
         int result = Test__InitSeq(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "_InitSeq", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "streq.c")){ 
         clock_t start = clock(); 
         int result = Test_streq(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "streq", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "strieq.c")){ 
         clock_t start = clock(); 
         int result = Test_strieq(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "strieq", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_GetPatchInfo_Simple.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_GetPatchInfo_Simple(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_GetPatchInfo_Simple", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_CheckConflict_Blacklist.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_CheckConflict_Blacklist(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_CheckConflict_Blacklist", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_CheckConflict_Pass.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_CheckConflict_Pass(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_CheckConflict_Pass", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_CheckCompat_WrongMLVer.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_CheckCompat_WrongMLVer(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_CheckCompat_WrongMLVer", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_CheckCompat_Pass.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_CheckCompat_Pass(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_CheckCompat_Pass", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_CheckDep_Pass.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_CheckDep_Pass(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_CheckDep_Pass", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_CheckDep_Fail.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_CheckDep_Fail(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_CheckDep_Fail", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_PatchKeyExists_Pass.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_PatchKeyExists_Pass(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_PatchKeyExists_Pass", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_PatchKeyExists_Fail.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_PatchKeyExists_Fail(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_PatchKeyExists_Fail", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_InstallPatch_Repl.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_InstallPatch_Repl(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_InstallPatch_Repl", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_GetPatchInfo_ClearExisting.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_GetPatchInfo_ClearExisting(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_GetPatchInfo_ClearExisting", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_InstallPatch_ClearExisting.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_InstallPatch_ClearExisting(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_InstallPatch_ClearExisting", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Var_MakeEntry.c")){ 
         clock_t start = clock(); 
         int result = Test_Var_MakeEntry(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Var_MakeEntry", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Var_Exists.c")){ 
         clock_t start = clock(); 
         int result = Test_Var_Exists(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Var_Exists", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Var_GetType.c")){ 
         clock_t start = clock(); 
         int result = Test_Var_GetType(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Var_GetType", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Var_GetValue_SQL.c")){ 
         clock_t start = clock(); 
         int result = Test_Var_GetValue_SQL(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Var_GetValue_SQL", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Var_GetType_SQL.c")){ 
         clock_t start = clock(); 
         int result = Test_Var_GetType_SQL(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Var_GetType_SQL", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Var_UpdateEntry.c")){ 
         clock_t start = clock(); 
         int result = Test_Var_UpdateEntry(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Var_UpdateEntry", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_Install_UnitTest_repl.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_Install_UnitTest_repl(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_Install_UnitTest_repl", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_Install_UnitTest_clear_existing.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_Install_UnitTest_clear_existing(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_Install_UnitTest_clear_existing", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_Install_UnitTest_variable_simple.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_Install_UnitTest_variable_simple(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_Install_UnitTest_variable_simple", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }
    if(streq(input, "Mod_Install_UnitTest_VarRepatch.c")){ 
         clock_t start = clock(); 
         int result = Test_Mod_Install_UnitTest_VarRepatch(); 
         clock_t end = clock(); 
         const char *verdict = result ? "PASS" : "FAIL"; 
         printf("[%s] %s (%f s)\n", verdict, "Mod_Install_UnitTest_VarRepatch", ((float)(end-start))/CLOCKS_PER_SEC); 
         return !result; 
    }


    printf("[FAIL] %s not found\n", input);
    return 1;
}

int Test_Test_Caller(){
    // Nasty side-effect...
    return 0;
}
