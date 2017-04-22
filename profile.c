/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "includes.h"              // LOCAL: All includes
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_FindMetadata()
*  Description:  Make a profile config file from inputs.
*                Filename is dictated by value of "profile".
* =====================================================================================
*/
void Profile_Save(
	const char *profile,
	const char *game,
	const char *gameConf,
	int checksum,
	const char *runpath,
	const char *gamever
){
	json_t *Config = json_pack(
		"{s:s, s:s, s:i, s:s, s:s}",
		"GamePath", game,
		"MetadataPath", gameConf,
		"Checksum", checksum,
		"RunPath", runpath,
		"GameVer", gamever
	);
	json_dump_file(Config, profile, 0);
	json_decref(Config);
	return;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_FindMetadata()
*  Description:  Find the metadata JSON that corresponds with the given game EXE.
* =====================================================================================
*/
#ifdef _WIN32
char * Profile_FindMetadata(const char *gamePath)
{
	WIN32_FIND_DATA output;
	HANDLE hCfg;
	BOOL ResultCode;
	char *MetadataPath = NULL;
	
	//Set working directory to program directory
	chdir(CONFIG.PROGDIR);
	//For each metadata file in /games...
	hCfg = FindFirstFile(".\\games\\*.json", &output);
	if(hCfg == INVALID_HANDLE_VALUE){
		return NULL;
	} else {
		ResultCode = 1;
	}
	
	while(ResultCode != 0){
		json_t *CurrCfg, *whitelist;
		json_error_t error;
		char *GameEXE = NULL;

		//Compose directory name
		asprintf(&MetadataPath, "%s\\games\\%s", CONFIG.PROGDIR, output.cFileName);
		
		//Load "main file" name and whitelist
		CurrCfg = json_load_file(MetadataPath, 0, &error);
		if(CurrCfg == NULL){
			chdir(CONFIG.PROGDIR);
			ResultCode = FindNextFile(hCfg, &output);
			continue;
		}
		
		whitelist = json_object_get(CurrCfg, "Whitelist");
		if(whitelist == NULL){
			chdir(CONFIG.PROGDIR);
			ResultCode = FindNextFile(hCfg, &output);
			json_decref(CurrCfg);
			continue;
		}
		
		GameEXE = JSON_GetStr(CurrCfg, "GameEXE");

		//Set working directory to game directory
		chdir(gamePath);
		
		// Make sure it has the correct game file
		if(
			File_Exists(GameEXE, TRUE, FALSE) &&
			(File_WhitelistIndex(GameEXE, whitelist) != -1)
		){
			safe_free(GameEXE);
			json_decref(CurrCfg);
			break;
		}
		
		//Nope, not it. Load next file.
		safe_free(GameEXE);
		json_decref(CurrCfg);
		chdir(CONFIG.PROGDIR);
		ResultCode = FindNextFile(hCfg, &output);
	}
	if(GetLastError() == ERROR_NO_MORE_FILES){
		MetadataPath = NULL;
	}
	FindClose(hCfg);
	return MetadataPath;
}
#endif

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_DumpLocal()
*  Description:  Dumps the given configration struct into the file in the CURRPROF
*                member.
* =====================================================================================
*/
void Profile_DumpLocal(struct ProgConfig *LocalConfig)
{
	Profile_Save(
		LocalConfig->CURRPROF,
		LocalConfig->CURRDIR,
		LocalConfig->GAMECONFIG,
		LocalConfig->CHECKSUM,
		LocalConfig->RUNPATH,
		LocalConfig->GAMEVER
	);
	return;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_Load()
*  Description:  Takes a path to a json configuration file in and returns a struct
*                containing it's value. Also calculates the checksum to make sure that
*                it's valid.
* =====================================================================================
*/
struct ProgConfig * Profile_Load(char *fpath)
{
	struct ProgConfig *LocalConfig;
	json_t *GameCfg, *Profile;
	
	//Init struct
	LocalConfig = malloc(sizeof(struct ProgConfig));
	if (!LocalConfig) {
		CURRERROR = errCRIT_MALLOC;
		return NULL;
	}
	memset(LocalConfig, 0, sizeof(*LocalConfig));
	
	//Load profile
	Profile = JSON_Load(fpath);
	if(!Profile){
		json_decref(Profile);
		return LocalConfig;
	}
	
	//Set variables
	LocalConfig->CURRPROF = strdup(fpath);
	LocalConfig->GAMECONFIG = JSON_GetStr(Profile, "MetadataPath");
	LocalConfig->CURRDIR = JSON_GetStr(Profile, "GamePath");
	LocalConfig->RUNPATH = JSON_GetStr(Profile, "RunPath");
	LocalConfig->PROGDIR = strdup(CONFIG.PROGDIR);
	LocalConfig->GAMEVER = JSON_GetStr(Profile, "GameVer");
	LocalConfig->CHECKSUM = JSON_GetuInt(Profile, "Checksum");
	
	//Load game config
//	chdir(CONFIG.PROGDIR);
	GameCfg = JSON_Load(LocalConfig->GAMECONFIG);
	if(!GameCfg){
		//Seems config is broken/missing
		json_decref(GameCfg);
		json_decref(Profile);
		Profile_EmptyStruct(LocalConfig);
		return NULL;
	}
	//Get game UUID
	LocalConfig->GAMEUUID = JSON_GetStr(GameCfg, "GameUUID");
	
	// Verify cheksum
	if (!Profile_ChecksumAlert(LocalConfig)) {
		json_decref(GameCfg);
		json_decref(Profile);
		Profile_EmptyStruct(LocalConfig);
		return NULL;
	}
	
	//All good!
	json_decref(GameCfg);
	json_decref(Profile);
	return LocalConfig;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_ChecksumAlert()
*  Description:  Displays a message if the checksum in the given profile is not
*                equal to the actual checksum of the file.
* =====================================================================================
*/
BOOL Profile_ChecksumAlert(
	struct ProgConfig *LocalConfig
){
	char *GameEXE = Profile_GetGameEXE(LocalConfig->GAMECONFIG);
	char *GamePath = NULL;
	unsigned int checksum;

	asprintf(&GamePath, "%s/%s", LocalConfig->CURRDIR, GameEXE);
	checksum = crc32File(GamePath);
	safe_free(GamePath);

	if (LocalConfig->CHECKSUM != checksum && LocalConfig->CHECKSUM != 0) {
		//Bad checksum. Ugh.
		char *msg = NULL;
		asprintf(&msg, "%s has been changed outside the mod loader!\n\n"
			"You will need to:\n"
			"1. manually restore your game backup,\n"
			"2. remove the '.db' file in your game's install folder,\n"
			"3. create a new profile.",
			GameEXE);
		AlertMsg(msg, "Checksum error");
		safe_free(msg);
		safe_free(GameEXE);
		return FALSE;
	}
	safe_free(GameEXE);
	return TRUE;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_GetGameEXE()
*  Description:  Gets the executable path corresponding to the game selected in the
*                game config file
* =====================================================================================
*/
char * Profile_GetGameEXE(const char *cfgpath)
{
	char *GameEXE;
	json_t *CurrCfg;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		//return strdup("");
		return NULL;
	}

	GameEXE = JSON_GetStr(CurrCfg, "GameEXE");
	json_decref(CurrCfg);
	return GameEXE;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_GetGameVer()
*  Description:  Gets the friendly version name corresponding to the game selected
*                in the game config file provided
* =====================================================================================
*/
char * Profile_GetGameVer(const char *cfgpath, struct ProgConfig *LocalConfig)
{
	char *GameVer, *GameEXE, *GamePath;
	json_t *CurrCfg, *whitelist;

	CurrCfg = JSON_Load(cfgpath);
	if (CurrCfg == NULL) {
		//return strdup("");
		return NULL;
	}

	whitelist = json_object_get(CurrCfg, "Whitelist");
	if (whitelist == NULL) {
		json_decref(CurrCfg);
		//return strdup("");
		return NULL;
	}

	//Could call funct, but really this is simpler.
	GameEXE = JSON_GetStr(CurrCfg, "GameEXE");
	asprintf(&GamePath, "%s/%s", LocalConfig->CURRDIR, GameEXE);

	if (strndef(LocalConfig->GAMEVER)) { // New profile
		GameVer = JSON_GetStr(
			json_array_get(
				whitelist,
				File_WhitelistIndex(GamePath, whitelist)
			), "Desc"
		);
	} else { // Loaded profile
		json_t *GameElem = JSON_FindArrElemByKeyValue(
			whitelist, "Name", json_string(LocalConfig->GAMEVER)
		);

		GameVer = JSON_GetStr(GameElem, "Desc");
	}
	safe_free(GameEXE);
	safe_free(GamePath);
	json_decref(CurrCfg);
	return GameVer;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_GetGameVerID()
*  Description:  Gets the internal version ID corresponding to the game selected in the
*                game config file provided
* =====================================================================================
*/
char * Profile_GetGameVerID(const char *cfgpath, struct ProgConfig *LocalConfig)
{
	char *GameVer, *GameEXE, *GamePath;
	json_t *CurrCfg, *whitelist;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return NULL;
	}

	whitelist = json_object_get(CurrCfg, "Whitelist");
	if(whitelist == NULL){
		json_decref(CurrCfg);
		return NULL;
	}

	//Could call funct, but really this is simpler.
	GameEXE = JSON_GetStr(CurrCfg, "GameEXE");
	asprintf(&GamePath, "%s/%s", LocalConfig->CURRDIR, GameEXE);

	if (strndef(LocalConfig->GAMEVER)) { // New profile
		GameVer = JSON_GetStr(
			json_array_get(
				whitelist,
				File_WhitelistIndex(GamePath, whitelist)
			), "Name"
		);
	}
	else { // Loaded profile
		GameVer = strdup(LocalConfig->GAMEVER);  // uhh...
	}

	safe_free(GameEXE);
	safe_free(GamePath);
	json_decref(CurrCfg);
	return GameVer;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_GetGameName()
*  Description:  Gets the name corresponding to the game selected in the
*                game config file provided
* =====================================================================================
*/
char * Profile_GetGameName(const char *cfgpath)
{
	char *GameName;
	json_t *CurrCfg;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		//return strdup("");
		return NULL;
	}
	
	GameName = JSON_GetStr(CurrCfg, "GameName");
	
	json_decref(CurrCfg);	
	return GameName;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_GetGameUUID()
*  Description:  Gets the UUID corresponding to the game selected in the
*                config file provided
* =====================================================================================
*/
char * Profile_GetGameUUID(const char *cfgpath)
{
	char *GameName;
	json_t *CurrCfg;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return NULL;
	}
	
	GameName = JSON_GetStr(CurrCfg, "GameUUID");
	
	json_decref(CurrCfg);	
	return GameName;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_EmptyStruct()
*  Description:  Empties a configuration struct by deallocating all variables within
* =====================================================================================
*/
void Profile_EmptyStruct(struct ProgConfig *LocalConfig)
{
	safe_free(LocalConfig->CURRDIR);
	safe_free(LocalConfig->PROGDIR);
	safe_free(LocalConfig->CURRPROF);
	safe_free(LocalConfig->GAMECONFIG);
	safe_free(LocalConfig->RUNPATH);
	safe_free(LocalConfig->GAMEVER);
	safe_free(LocalConfig->GAMEUUID);
	return;
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  Profile_Clone()
*  Description:  Creates a copy of the global configuration struct
* =====================================================================================
*/
struct ProgConfig * Profile_Clone()
{
	struct ProgConfig *LocalConfig = NULL;

	//Load configuration, if it exists
	if (!CONFIG.CURRPROF) {
		//First run
		//Keep all fields blank
		LocalConfig = malloc(sizeof(struct ProgConfig));
		if (!LocalConfig) {
			CURRERROR = errCRIT_MALLOC;
			return NULL;
		}

		memset(LocalConfig, 0, sizeof(*LocalConfig));
		LocalConfig->PROGDIR = strdup(CONFIG.PROGDIR);
		LocalConfig->CURRPROF = strdup("");
	}
	else {
		//Load profile!
		LocalConfig = Profile_Load(CONFIG.CURRPROF);
	}

	return LocalConfig;
}