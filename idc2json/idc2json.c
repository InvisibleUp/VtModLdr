// idc2json
// (c) 2016 InvisibleUp
// BSD 3-Clause license

// Converts function and variable information from an .idc dump
// from IDA (5.0, freeware) to a JSON usable by the Sonic R Mod Loader.
// I mean, did you think I was going to type in every address myself? Hah!
// (This is all kind of quick and dirty, so don't expect great code here.)

// Usage:
//	Rename [dump].idc to [dump].c 
//	In [dump].c, rename main(void) to idc_main(void)
//	Run `cc idc2json.c [dump].c` or equivalent
// This will produce an executable that will print a valid game JSON on stdout

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include "idc.h"

/* Struct to hold data info */
//We do know what we have before we get a name, so that's nice.
//We also don't have to care about what type it even is.

struct DataStore {
	int start;
	int end;
	char *name;
	char *comment;
	int segment;
};

struct DataStore *DataArray;
int DataArrayLen = 0;
int DataArrayAlloc = 16;

struct SegStore {
	int start;
	int end;
	bool isMem;
};
struct SegStore *SegArray;
int SegArrayLen = 0;
int SegArrayAlloc = 8;

char *EXEName = NULL;

/* Main functions that do the things that have to be done */

//Check if address exists. Returns index in array
int addrLoc(int startAddr){
	int i;
	for(i = 0; i < DataArrayLen; i++){
		if(DataArray[i].start == startAddr){
			return i;
		}
	}
	return -1;
}

//Get offset of address and allocate space if needed
int getAddrSpace(int addr){
	
	//Check
	int i = addrLoc(addr);
	if(i != -1){
		//Already exists
		return i;
	}
	
	//Realloc
	while(DataArrayLen + 1 > DataArrayAlloc){
		struct DataStore *DataArrayNew;
		DataArrayAlloc *= 2;
		DataArrayNew = realloc(
			DataArray, DataArrayAlloc * sizeof(struct DataStore)
		);
		if(DataArrayNew == NULL){
			//Malloc error
			exit(-1);
		}
		DataArray = DataArrayNew;
	}
	
	//Init
	memset(DataArray + DataArrayLen, 0, sizeof(struct DataStore));
	DataArray[DataArrayLen].start = addr;
	DataArrayLen++;
	
	return DataArrayLen - 1;
}


/* Wow! It's basically the same code as above. */
int segLoc(int startAddr){
	int i;
	for(i = 0; i < SegArrayLen; i++){
		if(SegArray[i].start == startAddr){
			return i;
		}
	}
	return -1;
}

//Get offset of segment and allocate space if needed
int getSegSpace(int addr){
	
	//Check
	int i = segLoc(addr);
	if(i != -1){
		//Already exists
		return i;
	}
	
	//Realloc (unlikely)
	while(SegArrayLen + 1 > SegArrayAlloc){
		struct SegStore *SegArrayNew;
		SegArrayAlloc *= 2;
		SegArrayNew = realloc(
			SegArray, SegArrayAlloc * sizeof(struct SegStore)
		);
		if(SegArrayNew == NULL){
			//Malloc error
			exit(-1);
		}
		SegArray = SegArrayNew;
	}
	
	//Init
	memset(SegArray + SegArrayLen, 0, sizeof(struct SegStore));
	SegArray[SegArrayLen].start = addr;
	SegArrayLen++;
	
	return SegArrayLen - 1;
}

//Actually new stuff. Find the segment that corresponds with the given address
//Returns offset in SegArray
int segFindAddr(int addr)
{
	int i = 0;
	for(i = 0; i < SegArrayLen; i++){
		if(SegArray[i].start > addr){
			return i - 1;
		}
	}
	return i;
}

void dumpArray(){
	json_t *out = json_array();
	for(int i = 0; i < DataArrayLen; i++){
		json_t *obj = NULL;
		char *file = NULL;
		if(!DataArray[i].name){continue;}
		if(DataArray[i].end == 0){continue;} //Ignore strings/local jumps
		
		//Get filename
		DataArray[i].segment = segFindAddr(DataArray[i].start); //Move this?
		if(SegArray[DataArray[i].segment].isMem){
			file = ":memory:";
		} else {
			file = EXEName;
		}
		
		if(DataArray[i].comment){
			obj = json_pack(
				"{ss ss si si ss}",
				"Name", DataArray[i].name,
				"Comment", DataArray[i].comment,
				"Start", DataArray[i].start,
				"End", DataArray[i].end - 1,
				"File", file
			);
		} else {
			obj = json_pack(
				"{ss si si ss}",
				"Name", DataArray[i].name,
				"Start", DataArray[i].start,
				"End", DataArray[i].end - 1,
				"File", file
			);
		}
		json_array_append(out, obj);
	}
	json_dump_file(out, "whitelist.json", JSON_INDENT(4) | JSON_PRESERVE_ORDER);
	json_decref(out);
}

int main(void){
	DataArray = malloc(DataArrayAlloc * sizeof(struct DataStore));
	SegArray = malloc(SegArrayAlloc * sizeof(struct SegStore));
	idc_main();
	//Dump JSON
	dumpArray();
}

/* Stub functions (because who cares about any of this stuff?) */

//Main
void DeleteAll(void){
	return;
}
void SetPrcsr(const char * a){
	return;
}
void StringStp(char a){
	return;
}
void Tabs(int a){
	return;
}
void Comments(int a){
	return;
}
void Voids(int a){
	return;
}
void XrefShow(int a){
	return;
}
void AutoShow(int a){
	return;
}
void Indent(int a){
	return;
}
void CmtIndent(int a){
	return;
}
void TailDepth(int a){
	return;
}

//Segments
void SetSelector(int a, int b){
	return;
}
void SegClass(int a, const char *b){
	return;
}
void SegDefReg(int a, const char *b, int c){
	return;
}
void SetSegmentType(int a, int b){
	return;
}
void LowVoids(int a){
	return;
}
void HighVoids(int a){
	return;
}

//Enums
int AddEnum(int a, const char *b, int c){
	return 0;
}
void AddConstEx(int id, const char *a, int b, int c){
	return;
}

//Structures
int AddStrucEx(int a, const char *b, int c){
	return 0;
}
int GetStrucIdByName(const char *a){
	return 0;
}
void AddStrucMember(int id, const char *a, int b, int c, int d, int e){
	return;
}
void AddStruct(int addr, const char *id){
	return;
}

//Bytes
void MakeStr(int addr, int end){
	return;
}
void MakeStruct(int a, const char *b){
	return;
}
void OpHex(int a, int b){
	return;
}
void OpDecimal(int a, int b){
	return;
}
void OpOctal(int a, int b){
	return;
}
void OpStkvar(int a, int b){
	return;
}
void OpBinary(int a, int b){
	return;
}
void OpOff(int a, int b, int c){
	return;
}
void MakeRptCmt(int addr, const char *a){
	//Import comments?
	return;
}
void OpEnumEx(int a, int b, int c, int d){
	return;
}
int GetEnum(const char *a){
	return 0;
}
void OpSign(int a, int b){
	return;
}
void OpNot(int a, int b){
	return;	
}

//Functions
void SetFunctionFlags(int addr, int flags){
	return;
}
void MakeFrame(int addr, int a, int b, int c){
	return;
}
void MakeLocal(int a, int b, const char *c, const char *d){
	return;
}
void MakeNameEx(int a, const char *b, enum MakeNameEx_Arg c){
	return;
}


/* Actual functions */

//Segments
void SegCreate(int start, int end, int id, int a, int b, int c){
	int i = getSegSpace(start);
	SegArray[i].end = end;
}
void SegRename(int addr, const char *name){
	int i = getSegSpace(addr);
	//Man this is going to be sketchy af
	if( //Definitely in EXE
		strcmp(name, "BEGTEXT") == 0 || //all code
		strcmp(name, ".rsrc") == 0 //Win32 resources
	){
		SegArray[i].isMem = false;
		
	} else if ( //Definitely in memory
		strcmp(name, "DGROUP") == 0 || //variables
		strcmp(name, ".bss") == 0 || //allocated memory
		strcmp(name, ".idata") == 0 //import data (could go either way)
	){
		SegArray[i].isMem = true;
		
	} else { //I have no clue. Assume EXE for safety reasons
		SegArray[i].isMem = true;
	}
}

//Bytes
//Order is MakeStr, MakeName for string constants (ignore)
//Order is MakeCode, MakeName, ... for functions
//Order is Make(D)word, MakeName for variables
//Order is MakeArray, MakeName for arrays
//Annoyingly comments come before anything else.

void ExtLinA(int addr, int id, const char *a){
	//Filename is the only thing I care about
	char *isOK = strstr(a, "; File Name");
	if(!isOK){return;}
	char *nameStart = strrchr(a, '\\');
	if(!nameStart){return;} //What platform is this??
	EXEName = strdup(nameStart+1);
	return;
}

void MakeComm(int addr, const char *comment){
	//Associate comment with saved address
	int i = getAddrSpace(addr);
	DataArray[i].comment = strdup(comment);
}

void MakeName(int addr, const char *name){
	//Associate name with saved address
	int i = getAddrSpace(addr);
	DataArray[i].name = strdup(name);
	return;
}

void MakeCode(int addr){
	// This function might not comply to valid C for standard opcodes.
	// Might have to replace 'x=0X' with '0X' in init phase
	getAddrSpace(addr);
	return;
}

void MakeArray(int addr, int len){
	int i = getAddrSpace(addr);
	DataArray[i].end = addr + len;
}

void MakeDword(int addr){
	int i = getAddrSpace(addr);
	DataArray[i].end = addr + 4;
}

void MakeWord(int addr){
	int i = getAddrSpace(addr);
	DataArray[i].end = addr + 2;
}

void MakeFloat(int addr){
	int i = getAddrSpace(addr);
	DataArray[i].end = addr + 4;
}

void MakeDouble(int addr){
	int i = getAddrSpace(addr);
	DataArray[i].end = addr + 8;
}

void MakeByte(int addr){
	int i = getAddrSpace(addr);
	DataArray[i].end = addr + 1;
}

void MakeTbyte(int addr){
	//x87 80-bit value
	int i = getAddrSpace(addr);
	DataArray[i].end = addr + 10;
}

//Functions
void MakeFunction(int start, int end){
	int i = getAddrSpace(start);
	DataArray[i].end = end;
}
