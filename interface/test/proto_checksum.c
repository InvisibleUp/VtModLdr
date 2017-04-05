// Function prototype for checksum tests
// Return value is C boolean
// Tests the filename "filename" against the same file in the dir "testname"
// and makes sure it matches "expected". 

#include "../../includes.h"
#include "../../funcproto.h"

int Proto_Checksum(
	const char *filename, unsigned int expected, BOOL DispError
){
	unsigned int result;
	char *FilePath = NULL;

	asprintf(&FilePath, "%s/%s", CONFIG.PROGDIR, filename);
	if (!FilePath) {
		fprintf(stderr, "Memory allocation error! [Proto_Checksum asprintf 1]");
		return 1;
	}

	result = crc32File(FilePath);
	free(FilePath);
	
	if (result != expected && DispError) {
		printf(
			"Checksum of %s failed.\n"
			"Expected 0x%X \n"
			"Got      0x%X\n\n",
			filename, expected, result
		);
	}

	return result == expected;
}
