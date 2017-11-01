/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "includes.h"              // LOCAL: All includes
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages

///Profile stuff
//Make a profile config file from profile data
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

//Replaces SelectInstallFolder()
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
			(File_Known(GameEXE, whitelist) != -1)
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

//Dumps the current contents of global variables to CURRPROF
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

struct ProgConfig * Profile_Load(char *fpath)
{
	struct ProgConfig *LocalConfig;
	json_t *GameCfg, *Profile;
	char *GameEXE;
	
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
	
	//Load game config
//	chdir(CONFIG.PROGDIR);
	GameCfg = JSON_Load(LocalConfig->GAMECONFIG);
	if(!GameCfg){
		//Seems config is broken/missing
		json_decref(Profile);
		safe_free(LocalConfig->GAMECONFIG);
		safe_free(LocalConfig->CURRDIR);
		safe_free(LocalConfig->RUNPATH);
		return LocalConfig;
	}
	//Get game UUID
	LocalConfig->GAMEUUID = JSON_GetStr(GameCfg, "GameUUID");
	
	//Check if checksum is sane
	chdir(LocalConfig->CURRDIR);
	GameEXE = JSON_GetStr(GameCfg, "GameEXE");
	LocalConfig->CHECKSUM = crc32File(GameEXE);
// Thread-safe but complex version below
/*	{
		char *GamePath = NULL;
		asprintf(&GamePath, "%s/%s", LocalConfig->CURRDIR, GameEXE);
		LocalConfig->CHECKSUM = crc32File(GamePath);
		safe_free(GamePath);
	}*/
	if(
		LocalConfig->CHECKSUM != JSON_GetuInt(Profile, "Checksum") &&
		JSON_GetuInt(Profile, "Checksum") != 0
	){
		//Bad checksum. Ugh.
		char *msg = NULL;
		asprintf(&msg, "%s has been changed outside the mod loader!\n\n"
		"You will need to:\n"
		"1. manually restore your game backup,\n"
		"2. remove the '.db' file in your game's install folder,\n"
		"3. create a new profile.",
		GameEXE);
		AlertMsg(msg, "Checksum error");
		safe_free(GameEXE);
		safe_free(msg);
		
		//Clear game path and checksum
		LocalConfig->CHECKSUM = 0; //This might be dangerous. Let's see.
		safe_free(LocalConfig->CURRDIR);
		chdir(CONFIG.PROGDIR);
	}
	
	//All good!
	safe_free(GameEXE);
	json_decref(GameCfg);
	json_decref(Profile);
	return LocalConfig;
}


char * Profile_GetGameEXE(const char *cfgpath)
{
	char *GameEXE;
	json_t *CurrCfg;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return strdup("");
	}

	GameEXE = JSON_GetStr(CurrCfg, "GameEXE");
	json_decref(CurrCfg);
	return GameEXE;
}

char * Profile_GetGameVer(const char *cfgpath)
{
	char *GameVer, *GameEXE;
	json_t *CurrCfg, *whitelist;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return strdup("");
	}

	whitelist = json_object_get(CurrCfg, "Whitelist");
	if(whitelist == NULL){
		json_decref(CurrCfg);
		return strdup("");
	}

	//Could call funct, but really this is simpler.
	GameEXE = JSON_GetStr(CurrCfg, "GameEXE");

	GameVer = JSON_GetStr(
		json_array_get(
			whitelist,
			File_Known(GameEXE, whitelist)
		), "Desc"
	);

	safe_free(GameEXE);
	json_decref(CurrCfg);
	return GameVer;
}

char * Profile_GetGameVerID(const char *cfgpath)
{
	char *GameVer, *GameEXE;
	json_t *CurrCfg, *whitelist;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return strdup("");
	}

	whitelist = json_object_get(CurrCfg, "Whitelist");
	if(whitelist == NULL){
		json_decref(CurrCfg);
		return strdup("");
	}

	//Could call funct, but really this is simpler.
	GameEXE = JSON_GetStr(CurrCfg, "GameEXE");

	GameVer = JSON_GetStr(
		json_array_get(
			whitelist,
			File_Known(GameEXE, whitelist)
		), "Name"
	);
	safe_free(GameEXE);
	json_decref(CurrCfg);
	return GameVer;
}

char * Profile_GetGameName(const char *cfgpath)
{
	char *GameName;
	json_t *CurrCfg;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return strdup("");
	}
	
	GameName = JSON_GetStr(CurrCfg, "GameName");
	
	json_decref(CurrCfg);	
	return GameName;
}

char * Profile_GetGameUUID(const char *cfgpath)
{
	char *GameName;
	json_t *CurrCfg;
	
	CurrCfg = JSON_Load(cfgpath);
	if(CurrCfg == NULL){
		return strdup("");
	}
	
	GameName = JSON_GetStr(CurrCfg, "GameUUID");
	
	json_decref(CurrCfg);	
	return GameName;
}

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