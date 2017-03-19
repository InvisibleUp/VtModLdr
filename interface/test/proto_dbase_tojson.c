// Takes a no-argument SQL query and converts it to JSON

#include "../../includes.h"
#include "../../funcproto.h"

json_t * Proto_DBase_ToJSON(const char *query) {
	sqlite3_stmt *command;
	json_t *out;

	if (SQL_HandleErrors(__FILE__, __LINE__,
		sqlite3_prepare_v2(CURRDB, query, -1, &command, NULL)
	) != 0) {
		return NULL;
	}
	out = SQL_GetJSON(command);
	if (SQL_HandleErrors(__FILE__, __LINE__, sqlite3_finalize(command)) != 0) {
		return NULL;
	}

	return out;
}