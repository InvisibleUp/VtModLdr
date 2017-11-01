///List of canned error messages
// 8 char tab, 80 column hard limit. 3 lines maximum per message.

///Generic Error Messages
//Critical s
const char *errormsg_Crit_Dbase = 
	"This program's internal database has encountered a critical error.\n"
	"The operation had been cancelled.";
const char *errormsg_Crit_Sys =
        "Something has gone HORRIBLY WRONG with your system and this program\n"
        "must exit now. Save all your open files and REBOOT NOW!\n";
const char *errormsg_Crit_Funct =
	"A function that should not have failed just did. Alert the\n"
	"program's developer of this issue.";
const char *errormsg_Crit_Argmnt =
	"The program's developer made a mistake when calling a function.\n"
	"Alert the program's developer of this issue.";
const char *errormsg_Crit_Malloc =
	"Your system is out of memory.\n"
	"Save all your open files and REBOOT NOW!";

//Warnings
const char *errormsg_Wng_NoSpc =
	"There is no space left in the file for patches. You will need to\n"
	"uninstall some mods or install some space-clearing mods.";
const char *errormsg_Wng_BadDir =
	"The directory selected does not exist or has things we didn't\n"
	"think would be there. You will need to choose a different directory.";
const char *errormsg_Wng_BadFile =
	"The file you selected does not exist or is not the file we needed.\n"
	"You will need to choose a different file.";
const char *errormsg_Wng_Config =
	"There was an issue reading the program's configuration file.\n"
	"We will reset the effected settings to the defaults.";
const char *errormsg_Wng_ReadOnly =
	"The file or directory you selected is marked as read-only.\n"
	"Make sure the file is not on a CD or restricted by an administrator.";
const char *errormsg_Wng_ModCfg =
	"This mod appears to have an issue. Installation cannot continue.\n\n"
	"We do not know what the issue is, but contact the mod developer.";

///Configuration errors
//Game metadata file
const char *errormsg_Cfg_Game =
        "The metadata file for this game does not have a whitelist of\n"
        "valid game files. We can't tell what files this game has.\n\n"
	"You will not be able to install mods until you get a new\n"
	"version of the metadata file for this game.\n";

///Game directory errors
//Game's directory is missing expected files
const char *errormsg_GameDir_BadContents = 
        "This directory does not have the files for the game you selected.\n"
        "You will need to choose a directory that has that game in it.";
	
///Mod installation error
//Cannot continue preamble
const char *errormsg_ModInst_CantCont =
        "This mod appears to have an issue. Installation cannot continue.\n\n"
        "Please contact the mod developer with the following information:\n"
        "%s\t- ";