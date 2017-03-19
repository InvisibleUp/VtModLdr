// Tests if CRC32_File function works at all

#include "../../includes.h"
#include "../../funcproto.h"

int Test_asprintf_Basic()
{
	const char *expected = "Hello World!";
	const char *world = "World";
	char *out = NULL;
	int result;

	asprintf(&out, "%s %s!", "Hello", world);

	if (!out) {
		fprintf(stderr, "Memory allocation error! [Test_asprintf_Basic asprintf 1]");
		return 1;
	}

	result = streq(out, expected);
	free(out);

	return result;
		
}
