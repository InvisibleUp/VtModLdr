// Tests program init sequence, including proper generation 
// of profile, dummy file, and database

#include "../../includes.h"
#include "../../funcproto.h"

int Test__InitSeq()
{
	int result;

	// Test dummy file checksum
	result = Proto_Checksum("test.bin", 0xE20EEA22, TRUE);
	if (result == FALSE) { 
        return FALSE; 
    }
    
	// Test mod db
	result = Proto_DBase_OK();
	if (result == FALSE) { return FALSE; }

	return TRUE;

}
