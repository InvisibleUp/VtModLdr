// Function prototype for checksum tests
// Return value is C boolean

#include "../../includes.h"
#include "../../funcproto.h"

int Proto_Checksum(
	const char *testname, const char *filename, unsigned int expected
){
	unsigned int result;
	char *FilePath = NULL;

	asprintf(&FilePath, "%s/test/%s/%s", CONFIG.PROGDIR, testname, filename);
	if (!FilePath) {
		fprintf(stderr, "Memory allocation error! [Proto_Checksum asprintf 1]");
		return 1;
	}

	result = crc32File(FilePath);
	free(FilePath);
	
	if (result != expected) {
		char *msg;

		asprintf(
			&msg,
			"Checksum of %s failed.\n"
			"Expected %X\n"
			"Got %X",
			filename, expected, result
		);
	}

	return result == expected;
}
