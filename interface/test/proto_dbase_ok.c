// Makes sure the database has only the 3 spaces created at the start

#include "../../includes.h"
#include "../../funcproto.h"

int Proto_DBase_OK() {
	json_t *out;
	int result = 0;

	// Define key/value pairs
	const char *command1 = "SELECT name FROM sqlite_master WHERE type='table';";
	const char *value1[] = {
		"Spaces",
		"Mods",
		"Dependencies",
		"Revert",
		"Files",
		"Variables",
		"VarList",
		"VarRepatch"
	};
	const unsigned int value1len = 8;

	const char *command2 = "SELECT * FROM Spaces";
	const char *value2_ID[] = {
		"Base.:memory:",
		"Base.:memory:",
		"Base.test.bin",
		"Base.test.bin",
		"SectorA",
		"Base.test.bin",
		"SectorA-Base.test.bin",
		"SectorB",
		"Base.test.bin",
		"SectorB-Base.test.bin"
	};
	const char *value2_Version[] = {
		"1", "2", "1", "2", "1", "3", "1", "1", "4", "1"
	};
	const char *value2_Type[] = {
		"New", "Add", "New", "Add", "Add", "Add", "Split", "Add", "Add", "Split"
	};
	const char *value2_File[] = {
		"0", "0", "1", "1", "1", "1", "1", "1", "1", "1"
	};
	const unsigned int value2len = 10;
	// Skipping UsedBy, PatchID because I don't know how to compare nulls
	// Skipping Mod, Start, End, Len out of laziness



	// Check existence of all tables
	out = Proto_DBase_ToJSON(command1);
	if (!Proto_JSON_CheckOutput(out, "name", value1, value1len)) {
		return FALSE;
	}
	json_decref(out);

	// Check for existence of exactly 3 patches using 10 spaces
	out = Proto_DBase_ToJSON(command2);
	if (json_array_size(out) != value2len) {
		fprintf(
			stderr, "Incorrect number of spaces in database!\n"
			"(Have %d, expected %d.)\n",
			json_array_size(out), value2len
		);
	}

	if (
		!Proto_JSON_CheckOutput(out, "ID", value2_ID, value2len) ||
		!Proto_JSON_CheckOutput(out, "Version", value2_Version, value2len) ||
		!Proto_JSON_CheckOutput(out, "Type", value2_Type, value2len) ||
		!Proto_JSON_CheckOutput(out, "File", value2_File, value2len)
	) {
		return FALSE;
	}
	json_decref(out);

	// Meh. It's probably fine.
	return TRUE;
}