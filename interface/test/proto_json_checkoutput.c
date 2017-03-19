// Checks a JSON output against a list of known keys

#include "../../includes.h"
#include "../../funcproto.h"

int Proto_JSON_CheckOutput(
	json_t *out,
	const char *keyname,
	const char *values[],
	const unsigned int vallen
) {
	json_t *row;
	int result = TRUE;
	size_t i;

	json_array_foreach(out, i, row) {
		char *string = JSON_GetStr(row, keyname);
		if (i > vallen) {
			fprintf(stderr, "Too many elements in JSON output\n");
		}

		if (strneq(string, values[i])) {
			fprintf(
				stderr,
				"JSON output bad\n"
				"(at index %d expected %s but saw %s)\n",
				i, values[i], string
			);
			result = FALSE;
		}
		free(string);
	}
	
	return result;
}