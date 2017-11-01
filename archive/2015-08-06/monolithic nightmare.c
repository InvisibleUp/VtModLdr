int SelectInstallFolder()
{
	int errorcode, i, handle;
	long fsize;
	unsigned long checksum;
	
	//List of errorcodes:
	//-1= unknown error
	//0 = none
	//1 = no directory
	//2 = incorrect directory
	//3 = read/write error
	//4 = incorrect filesize
	//5 = incorrect checksum
	
	errorcode = SelectFolder("Pick Sonic R Installation Directory");
	// User refuses to give a directory
	if (errorcode == 1) {
		PostQuitMessage(0);
		return errorcode;
	}

	// Move to the directory. We'll be there a while.
	chdir(path);
	
	// Make sure it has a SONICR.EXE
	if (access("SONICR.EXE", F_OK | W_OK) != 0){
		switch(errno){
		case ENOENT:
			MessageBox (0, 
			"This folder does not contain a Sonic R installation.\n"
			"Make sure you've picked one that contains 'SONICR.EXE'\n"
			"and all required files.",
			"Incorrect Folder", MB_ICONEXCLAMATION | MB_OK);
			errorcode = 2;
		return errorcode;
		case EACCES:
			MessageBox (0, 
			"This folder is read-only. Make sure this folder is\n"
			"on your hard disk and not on a CD, as this program\n"
			"cannot patch straight to a CD.",
			"Read-Only Folder", MB_ICONERROR | MB_OK);
			errorcode = 3;
		return errorcode;
		}
	}
	
	//Now we open the file so we can read it.
	handle = _open("SONICR.EXE", _O_BINARY|_O_RDWR);
	if (handle == -1){
		MessageBox (0, 
			"An unknown error occurred when opening SONICR.EXE.\n"
			"Make sure your system is not low on resources and try again.\n",
			"General I/O Error", MB_ICONERROR | MB_OK);
			errorcode = -1;
			return errorcode;
	}
	//Might as well get the filesize while we're here.
	fsize = filelength(handle);
	
	//Make sure SONICR.EXE is actually a copy of Sonic R via filesize
	for (i = 0; i < ValidEXECount; i++){
		if (fsize == ValidEXEList[i].size){
			errorcode = 0;
			break;
		} else { errorcode = 4; }
	}
	//Return if the file is invalid
	if (errorcode == 4){
		MessageBox (0, 
			"The SONICR.EXE file appears to be the wrong size.\n"
			"This may mean you are running an incompatible version of the game.\n"
			"Note that the network patch is currently unsupported in this release.\n"
			"If you got the game EXE from an official source, contact the creator\n"
			"of this mod loader. You may have a special revision!",
			"Incorrect Filesize", MB_ICONEXCLAMATION | MB_OK);
			return errorcode;
	}
		
	//Get the CRC32 of SONICR.EXE
	//At least it claims to be a CRC32. Doesn't match up with the shell util
	//Still does it's purpose though, so eh.
	checksum = crc32(handle, fsize);
	//_stprintf(szBuffer, _T("%u"), checksum);
	//MessageBox(0, szBuffer, _T("Info"), MB_OK);
	
	for (i = 0; i < ValidEXECount; i++){
		if (checksum == ValidEXEList[i].crc32){
			errorcode = 0;
			break;
		} else { errorcode = 5; }
	}
	//Return if the file is invalid
	if (errorcode == 5){
		MessageBox (0, 
			"The SONICR.EXE file does not match any known version.\n"
			"This may mean you are running an incompatible version of the game.\n"
			"Note that the network patch is currently unsupported in this release.\n"
			"If you got the game EXE from an official source, contact the creator\n"
			"of this mod loader. You may have a special revision!",
			"Incorrect Checksum", MB_ICONEXCLAMATION | MB_OK);
			return errorcode;
	}
	
	//That's about it. It's probably good now barring sudden meteorological catastrophe.
	return 0;
}