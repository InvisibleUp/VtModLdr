/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "includes.h"              // LOCAL: All includes
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_GetID
 *  Description:  Returns the ID number from a filename as stored in the SQL.
 * =====================================================================================
 */
int File_GetID(const char * FileName)
{
	sqlite3_stmt *command;
	int ID = -1;
	const char *query1 = "SELECT ID FROM Files WHERE Path = ?;";

	CURRERROR = errNOERR;

	//Check if using memory psuedofile
	if(strieq(FileName, ":memory:")){
		return 0;
	}
	
	//Get the ID
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, FileName, -1, SQLITE_STATIC)
	) != 0){CURRERROR = errCRIT_DBASE; return -1;}
	
	ID = SQL_GetNum(command);
	
	if(CURRERROR != errNOERR){return -1;}
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return -1;
	}

	if(ID == -1){
		//Add new entry into DB
		ID = File_MakeEntry(FileName);
		//ID might be -1. Return that and let caller deal with it.
	}
	
	return ID;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_GetName
 *  Description:  Returns the filename from the file ID as stored in the SQL.
 * =====================================================================================
 */
char * File_GetName(int input)
{
	sqlite3_stmt *command;
	const char *query = "SELECT Path FROM Files WHERE ID = ?;";
	char *output = NULL;
	CURRERROR = errNOERR;
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_int(command, 1, input)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		//return strdup("");
		return NULL;
	}
	
	output = SQL_GetStr(command);
	
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		safe_free(output);
		//return strdup("");
		return NULL;
	}
	command = NULL;
	return output;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_GetPath
 *  Description:  Returns the file path from the file ID as stored in the SQL.
 * =====================================================================================
 */
char * File_GetPath(int input)
{
	char *FileName = NULL;
	char *output = NULL;

	FileName = File_GetName(input);
	if(strndef(FileName)){
		return NULL;
	}

	asprintf(&output, "%s\\%s", CONFIG.CURRDIR, FileName);
	safe_free(FileName);

	return output;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_MakeEntry
 *  Description:  For the given file name, creates an entry in the "Files" SQL table.
 *                Also adds a preliminary CLEAR space spanning the length of the file.
 * =====================================================================================
 */
int File_MakeEntry(const char *FileName){
	sqlite3_stmt *command;
	const char *query1 = "SELECT MAX(ID) FROM Files;";
	const char *query2 = "INSERT INTO Files (ID, Path) VALUES (?, ?);";
	int IDCount, ID, FileLen;
	char *FilePath = NULL;
	struct ModSpace ClearSpc = {0};
	
	CURRERROR = errNOERR;
    
    //If :memory:, create virtual file with 2GB max length
	//(2GB is the default max address space for 32-bit Windows programs)
	if(streq(FileName, ":memory:")){
		FileLen = INT_MAX;
	} else {
        //Get size of file
        asprintf(&FilePath, "%s/%s", CONFIG.CURRDIR, FileName);
        FileLen = filesize(FilePath);
        safe_free(FilePath);
    }

	//If the file doesn't exist, there's an obvious mod configuration error
	if(FileLen <= 0){
		CURRERROR = errWNG_MODCFG;
		return -1;
	}
	
	//Get highest ID assigned
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query1, -1, &command, NULL)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		return -1;
	}
	   
	IDCount = SQL_GetNum(command);
	
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		return -1;
	}
	//Check that GetNum succeeded
	if(IDCount == -1 || CURRERROR != errNOERR){
		return -1;
	}
	
	//New ID is highest ID + 1 (unless file is :memory:)
	//Pitfall here: No file entry found and no file entries peroid
	//both return 0.
	if(streq(FileName, ":memory:")){
		ID = 0;
	} else {
		ID = IDCount + 1;;
	}
	
	//Insert new ID into database
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query2, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_int(command, 1, ID) |
		sqlite3_bind_text(command, 2, FileName, -1, SQLITE_STATIC)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_step(command)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE; return -1;
	}
	command = NULL;
	
	// Add a NEW space and a big ol' CLEAR space spanning the whole file
	asprintf(&ClearSpc.ID, "Base.%s", FileName);
	ClearSpc.FileID = ID; //;)
	ClearSpc.Start = 0;
	ClearSpc.End = FileLen;
	ClearSpc.Valid = TRUE;
	
	Mod_MakeSpace(&ClearSpc, "MODLOADER@invisibleup", "New");
	Mod_MakeSpace(&ClearSpc, "MODLOADER@invisibleup", "Clear");
	
	safe_free(ClearSpc.ID);
	
	//Return new ID in case the caller needs it
	return ID;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_FindPatchOwner
 *  Description:  Returns the filename that contains the patch PatchUUID.
 *                Is distinct from Mod_FindPatchOwner.
 * =====================================================================================
 */
char * File_FindPatchOwner(const char *PatchUUID)
{
	char *out = NULL;
	
	sqlite3_stmt *command;
	const char *query = "SELECT Path FROM Files "
	                    "JOIN Spaces ON Files.ID = Spaces.File "
	                    "WHERE Spaces.ID = ?";
	CURRERROR = errNOERR;
	
	if(SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0 || SQL_HandleErrors(__FILE__, __LINE__, 
		sqlite3_bind_text(command, 1, PatchUUID, -1, SQLITE_STATIC)
	) != 0){
		CURRERROR = errCRIT_DBASE;
		//return strdup("");
		return NULL;
	}
	
	out = SQL_GetStr(command);
	
	if(CURRERROR != errNOERR){
		safe_free(out);
		//return strdup("");
		return NULL;
	}
	if(SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0){
		CURRERROR = errCRIT_DBASE;
		safe_free(out);
		//return strdup("");
		return NULL;
	}
	return out;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_IsPE
 *  Description:  Examines the given file to determine if it is a Windows
 *                (or compatible) PE executable file. Returns TRUE if so.
 * =====================================================================================
 */
BOOL File_IsPE(const char *FilePath){
	int handle = -1;
	BOOL result = FALSE;

	//:memory: check
	if(strstr(FilePath, ":memory:")){
		return FALSE;
	}
	
	//Open file
	handle = File_OpenSafe(FilePath, _O_BINARY | _O_RDONLY);
	if(handle == -1){
		return FALSE;
	}
	
	//Check if DOS header is OK
	{
		char buf[3] = {0};
		read(handle, buf, 2);
		if(!streq(buf, "MZ")){
			goto File_IsPE_Return;
		}
	}
	
	//Seek to PE header (might be dangerous. Eh.)
	{
		uint32_t offset = 0;
		lseek(handle, 0x3C, SEEK_SET);
		read(handle, &offset, sizeof(offset));
		lseek(handle, offset, SEEK_SET);
	}
	
	//Check PE header
	{
		char buf[4] = {0};
		char check[4] = {'P', 'E', 0, 0};
		read(handle, buf, 4);
		if(memcmp(buf, check, 4) != 0){
			goto File_IsPE_Return;
		}
	}
	
	//We're probably fine at this point, barring somebody intentionally
	//trying to break this by feeding us MIPS binaries or something.
	result = TRUE;
	
File_IsPE_Return:
	close(handle);
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_PEToOff
 *  Description:  Converts Win32 PE memory location to raw file offset
 * =====================================================================================
 */
int File_PEToOff(const char *FilePath, uint32_t PELoc)
{
	int result = PELoc;
	int handle = -1;
	int i;
	int temp;
	
	uint32_t BaseAddr = 0;
	uint32_t PEHeaderOff = 0;
	
	uint16_t NoSections = 0;
	uint32_t NoDataDir = 0;
	
	uint32_t LastFileOff = 0;
	uint32_t LastPEOff = 0;
	
	//Check if PE or just some random file
	if(!File_IsPE(FilePath)){
		return result;
	}
	
	//Open file
	handle = File_OpenSafe(FilePath, _O_BINARY | _O_RDONLY);
	if(handle == -1){
		return result;
	}
	
	//Seek to first section header
	lseek(handle, 0x3C, SEEK_SET);
	read(handle, &PEHeaderOff, sizeof(PEHeaderOff));
	lseek(handle, PEHeaderOff, SEEK_SET);
	
	//Get base address (kind of tough)
	lseek(
		handle, 
		(4 + 2 + 2 + 4 + 4 + 4 + 2 + 2) + //COFF header 
		(2 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4),
		SEEK_CUR
	);
	temp = _tell(handle);
	read(handle, &BaseAddr, sizeof(BaseAddr));
	
	//Get section count
	lseek(handle, PEHeaderOff + 6, SEEK_SET);
	temp = _tell(handle);
	read(handle, &NoSections, sizeof(NoSections));
	
	//Skip past data directories
	lseek(
		handle, PEHeaderOff +
		4 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + //COFF header 
		2 + 1 + 1 + (4 * 9) + (2 * 6) + (4 * 4) +
		2 + 2 + (4 * 5), //mNumberOfRvaAndSizes
		SEEK_SET
	);
	read(handle, &NoDataDir, sizeof(NoDataDir));
	temp = _tell(handle);
	lseek(handle, 8 * NoDataDir, SEEK_CUR);
	temp = _tell(handle);
	
	//Parse section headers
	for(i = 0; i < NoSections; i++){
		uint32_t PERel, PEAbs, FileOff;
		
		//Get PE loc of section start
		lseek(handle, 12, SEEK_CUR);
		read(handle, &PERel, sizeof(PERel));
		PEAbs = PERel + BaseAddr;
		
		//Get file offset of section start
		lseek(handle, 4, SEEK_CUR);
		read(handle, &FileOff, sizeof(FileOff));
		
		//Go to next section header
		lseek(handle, 16, SEEK_CUR);
		
		//Check if we went past the section
		if(PELoc < PEAbs){break;}
		
		//Set Last* variables
		LastFileOff = FileOff;
		LastPEOff = PEAbs;
	}
	
	//Compute location (we're done!)
	if(i != 0){ //File offset is not before sections
		result = (PELoc - LastPEOff) + LastFileOff;
	}
	
	close(handle);
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_OffToPE
 *  Description:  Converts Win32 PE raw file offset to memory location
 * =====================================================================================
 */
int File_OffToPE(const char *FilePath, uint32_t FileLoc)
{
	int result = FileLoc;
	int handle = -1;
	int i = 0;
	
	uint32_t BaseAddr = 0;
	uint32_t PEHeaderOff = 0;
	
	uint16_t NoSections = 0;
	uint32_t NoDataDir = 0;
	
	uint32_t LastFileOff = 0;
	uint32_t LastPEOff = 0;
	
	//Check if PE or just some random file
	if(!File_IsPE(FilePath)){
		return result;
	}
	
	//Open file
	handle = File_OpenSafe(FilePath, _O_BINARY | _O_RDONLY);
	if(handle == -1){
		return result;
	}
	
	//Seek to first section header
	lseek(handle, 0x3C, SEEK_SET);
	read(handle, &PEHeaderOff, sizeof(PEHeaderOff));
	lseek(handle, PEHeaderOff, SEEK_SET);
	
	//Get base address (kind of tough)
	lseek(
		handle, 
		(4 + 2 + 2 + 4 + 4 + 4 + 2 + 2) + //COFF header 
		(2 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4),
		SEEK_CUR
	);
	read(handle, &BaseAddr, sizeof(BaseAddr));
	
	//Get section count
	lseek(handle, PEHeaderOff + 6, SEEK_SET);
	read(handle, &NoSections, sizeof(NoSections));
	
	//Skip past data directories
	lseek(
		handle, PEHeaderOff +
		4 + 4 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + //COFF header 
		2 + 1 + 1 + (4 * 8) + (2 * 6) + (4 * 4) +
		2 + 2 + (4 * 5), //mNumberOfRvaAndSizes
		SEEK_SET
	);
	read(handle, &NoDataDir, sizeof(NoDataDir));
	lseek(handle, 8 * NoDataDir, SEEK_CUR);
	
	//Parse section headers
	for(i = 0; i < NoSections; i++){
		uint32_t PERel, PEAbs, FileOff;
		
		//Get PE loc of section start
		lseek(handle, 12, SEEK_CUR);
		read(handle, &PERel, sizeof(PERel));
		PEAbs = PERel + BaseAddr;
		
		//Get file offset of section start
		lseek(handle, 4, SEEK_CUR);
		read(handle, &FileOff, sizeof(FileOff));
		
		//Go to next section header
		lseek(handle, 16, SEEK_CUR);
		
		//Check if we went past the section
		if(FileLoc < FileOff){break;}
		
		//Set Last* variables
		LastFileOff = FileOff;
		LastPEOff = PEAbs;
	}
	
	//Compute location (we're done!)
	result = (FileLoc - LastFileOff) + LastPEOff;
	
	close(handle);
	return result;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Exists
 *  Description:  Determine if a file is present in the current directory
 *                and sets CURRERROR if it is not. Also optionally warns
 *                if file is read-only.
 * =====================================================================================
 */

#ifndef HAVE_WINDOWS_H
BOOL File_Exists(const char *file, BOOL InFolder, BOOL ReadOnly)
{
	// TODO: How do we make this case-insensitive?
	int mode = ReadOnly ? (R_OK) : (R_OK | W_OK);
	CURRERROR = errNOERR;
	if (access(file, mode) != 0){
        
		switch(errno){
		case ENOENT: //File/Folder does not exist
			if(InFolder){CURRERROR = errWNG_BADDIR;}
			else{CURRERROR = errWNG_BADFILE;}
			errno = 0;
			return FALSE;
            
		case EACCES: // Read only error
			CURRERROR = errWNG_READONLY;
            errno = 0;
			return FALSE;
		}
	}
	return TRUE;
}
#else
BOOL File_Exists(const char *file, BOOL IsFolder, BOOL ReadOnly)
{
//	int mode = ReadOnly ? (R_OK) : (R_OK | W_OK);
	int retval = -1;
	CURRERROR = errNOERR;

	retval = GetFileAttributes(file);
	if (retval == -1) {
		// File not found (most likely)
		if (IsFolder) { CURRERROR = errWNG_BADDIR; }
		else { CURRERROR = errWNG_BADFILE; }
		return FALSE;
	}
	else if (ReadOnly && (retval & FILE_ATTRIBUTE_READONLY)) {
		// File is read-only
		CURRERROR = errWNG_READONLY;
		return FALSE;
	}
	return TRUE;
}
#endif

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_WhitelistIndex
 *  Description:  Tests if a file is found in the given whitelist by testing
 *                filesize and checksum. Returns index of version in whitelist.
 * =====================================================================================
 */
int File_WhitelistIndex(const char *FileName, json_t *whitelist)
{
	unsigned long fchecksum;
	signed long fsize;
	size_t i;
	json_t *value;
	CURRERROR = errNOERR;

	//Get filesize
	fsize = filesize(FileName);
	if(fsize == -1){
		CURRERROR = errWNG_BADFILE;
		return -1;
	}
	
	//Get the checksum of the file
	fchecksum = crc32File(FileName);
	if(fchecksum == 0){
		CURRERROR = errWNG_BADFILE;
		return -1;
	}
	
	//Report size and checksum
	/*{
		char *message = NULL;
		asprintf(&message,
			"%s\n CRC: %lX\n Size: %li",
			FileName, fchecksum, fsize);
		AlertMsg(message, "File Results");
		safe_free(message);
	}*/

	//Lookup file size and checksum in whitelist
	json_array_foreach(whitelist, i, value){
		char *temp = NULL;
		unsigned long ChkSum;
		signed long Size;

		temp = JSON_GetStr(value, "ChkSum");
		if (!temp) {
			return -1;
		}
		ChkSum = strtoul(temp, NULL, 16);
		Size = JSON_GetInt(value, "Size");
		safe_free(temp);
		
		if (fsize == Size && fchecksum == ChkSum) {
			return i;
		}
	}
	
	CURRERROR = errWNG_BADFILE;
	return -1;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_OpenSafe
 *  Description:  Safely opens a file by warning the user if the operation fails.
 *                Simple wrapper around io.h's _open function.
 * =====================================================================================
 */
int File_OpenSafe(const char *filename, int flags)
{
	int handle;
	int accessflag;
	//Check permissions first
	if((flags & _O_RDWR) || (flags & _O_WRONLY)){
		//Set flag to write access
		accessflag = W_OK | R_OK;
	} else {
		accessflag = R_OK;
	}
	/*if(access(filename, accessflag) != 0){
		ErrNo2ErrCode();
		return -1;
	}*/

	if (!File_Exists(filename, FALSE, (accessflag | W_OK))) {
		return -1;
	}
	
	handle = _open(filename, flags);
	//Make sure file is indeed open. (No reason it shouldn't be.)
	if (handle == -1){
		CURRERROR = errCRIT_FILESYS;
	}
	return handle;
}

#if defined(HAVE_WINDOWS_H)
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Copy
 *  Description:  Copies a file from [OldPath] to [NewPath].
 *                Simple wrapper around Windows's CopyFile function.
 *                Might be more complicated on POSIX systems.
 * =====================================================================================
 */
void File_Copy(const char *OldPath, const char *NewPath)
{
	CopyFile(OldPath, NewPath, FALSE);
}

#elif defined(HAVE_SYS_SENDFILE_H)
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Copy
 *  Description:  Copies a file from [OldPath] to [NewPath].
 *                Uses sendfile() in Linux 2.6+
 * =====================================================================================
 */
void File_Copy(const char *OldPath, const char *NewPath)
{
	int in = _open(OldPath, _O_RDONLY);
	int out = _open(NewPath, _O_WRONLY);
	int result;
	int len = filesize(OldPath);
	
	if(in == -1 || out == -1){
		ErrNo2ErrCode();
		return;
	}
	
	result = sendfile(out, in, NULL, len);

	ErrNo2ErrCode();
	close(in);
	close(out);
	return;
}
#endif

#if defined(HAVE_WINDOWS_H)
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Delete
 *  Description:  Delete [Path].
 *                Simple wrapper around Windows's DeleteFile function.
 *                Might be more complicated on POSIX systems.
 * =====================================================================================
 */
void File_Delete(const char *Path)
{
	DeleteFile(Path);
}

#elif defined(HAVE_UNISTD_H)
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Delete
 *  Description:  Delete [Path].
 *                Simple wrapper around unlink() on POSIX systems
 * =====================================================================================
 */
void File_Delete(const char *Path)
{
	unlink(Path);
}

#endif


#if defined(HAVE_WINDOWS_H)
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Deltree
 *  Description:  Deletes a directory and all files and folders within.
 *                If using Win32, it uses SHFileOperation. Input path must have
 *                terminating backslash.
 *                TODO: POSIX port needed
 * =====================================================================================
 */
BOOL File_DelTree(char *DirPath)
{
	SHFILEOPSTRUCT SHStruct = {0};
	char *DirPathZZ = NULL;
	int retval = 0;
	//asprintf(&DirPathZZ, "%s\0", DirPath);
	//My only possible guess
	DirPathZZ = strdup(DirPath);
	DirPathZZ[strlen(DirPath) - 1] = '\0';
	
	SHStruct.hwnd = NULL;
	SHStruct.wFunc = FO_DELETE;
	SHStruct.pFrom = DirPathZZ;
	SHStruct.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;

	retval = SHFileOperation(&SHStruct);
	safe_free(DirPathZZ);

	if(retval != 0 || SHStruct.fAnyOperationsAborted){
		//wut?
		return FALSE;
	}
	
	//Delete directory itself now, I think. (If not, oh well. No harm.)
	RemoveDirectory(DirPath);

	return TRUE;
}
#endif

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Movtree
 *  Description:  Moves a directory and all files and folders within.
 * =====================================================================================
 */
BOOL File_MovTree(char *srcPath, char *dstPath)
{
	//Actually stupidly simple. Cross-platform, too.
	return rename(srcPath, dstPath);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_Create
 *  Description:  Create the given file with the given length, filled with '\0' bytes.
 * =====================================================================================
 */
BOOL File_Create(char *FilePath, int FileLen)
{
	int handle = _creat(FilePath, S_IREAD | S_IWRITE);
	const char *pattern = "\0";
	
	if(handle == -1){
		ErrNo2ErrCode();
		return FALSE;
	}
	
	File_WritePattern(handle, 0, pattern, 1, FileLen);
	return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_WriteBytes
 *  Description:  Given an offset and a byte array of data write it to the given file.
 * =====================================================================================
 */
void File_WriteBytes(
	int filehandle,
	int offset,
	unsigned const char *data,
	int datalen
){
	lseek(filehandle, offset, SEEK_SET);
	write(filehandle, data, datalen);
	return;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  File_WritePattern
 *  Description:  Given an offset, length and byte pattern, it will write the pattern
 *                to the file until the length is filled.
 * =====================================================================================
 */
void File_WritePattern(
	int filehandle,
	int offset,
	unsigned const char *data,
	int datalen,
	int blocklen
){
	int blockRemain = blocklen;
	
	while(blockRemain > 0){
		File_WriteBytes(
			filehandle,
			offset,
			data,
			MIN(datalen, blockRemain)
		);
		blockRemain -= MIN(datalen, blockRemain);
		offset += MIN(datalen, blockRemain);
	}
}
