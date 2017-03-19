// Tests program init sequence, including proper generation 
// of profile, dummy file, and database

#include "../../includes.h"
#include "../../funcproto.h"

int Test__InitSeq()
{
	int result;
	const char *testname = "_InitSeq";
	char *RootPath = NULL;


	asprintf(&RootPath, "%s/test/_InitSeq", CONFIG.PROGDIR);
	if (!RootPath) {
		fprintf(stderr, "Memory allocation error! [1]");
	}

	// Test profile checksum
	result = Proto_Checksum(testname, "test_profile.json", 0xCD4DFDB5);
	if (result == FALSE) { return FALSE; }

	// Test dummy file checksum
	result = Proto_Checksum(testname, "test_profile.json", 0xCD4DFDB5);
	if (result == FALSE) { return FALSE; }

	// Test mod db
	result = Proto_DBase_OK();
	if (result == FALSE) { return FALSE; }

	return TRUE;

}
