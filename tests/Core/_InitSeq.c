// Tests program init sequence, including proper generation 
// of profile, dummy file, and database

#include "../../includes.h"
#include "../../funcproto.h"

int Test__InitSeq()
{
	int result;
	const char *testname = "_InitSeq";
	
    // Test config file checksum
	result = Proto_Checksum(testname, "test_profile.json", 0xCD4DFDB5);
	if (result == FALSE) { 
        return FALSE; 
    }

	// Test dummy file checksum
	result = Proto_Checksum(testname, "test.bin", 0xE20EEA22);
	if (result == FALSE) { 
        return FALSE; 
    }
    
	// Test mod db
	result = Proto_DBase_OK();
	if (result == FALSE) { return FALSE; }

	return TRUE;

}
