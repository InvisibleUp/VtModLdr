/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "includes.h"              // LOCAL: All includes
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages

///SQLite helper Functions
//////////////////////////

// I'm sorry, I'm not typing all this out every time.
// This is required, or else I get a segfault.
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_ColName
 *  Description:  Returns the name of a column given it's index number
 * =====================================================================================
 */
const char * SQL_ColName(sqlite3_stmt * stmt, int col){
	return sqlite3_column_name(stmt, col) == NULL ?
		"" : (char *)sqlite3_column_name(stmt, col);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_ColText
 *  Description:  Returns the value of an item given it's column index number
 * =====================================================================================
 */
const char * SQL_ColText(sqlite3_stmt * stmt, int col){	
	return sqlite3_column_text(stmt, col) == NULL ?
		"" : (char *)sqlite3_column_text(stmt, col);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_GetJSON
 *  Description:  Converts the result of an SQL statement to a easily searchable
 *                or savable JSON array.
 * =====================================================================================
 */
json_t * SQL_GetJSON(sqlite3_stmt *stmt)
{
	int i, errorNo;
	json_t *result = json_array();
	CURRERROR = errNOERR;
	
	///Compose the JSON array
	errorNo = sqlite3_step(stmt);
        while (errorNo == SQLITE_ROW){
		//For each row of results, create a new object
		json_t *tempobj = json_object();
		
		for(i = sqlite3_column_count(stmt) - 1; i >= 0; i--){
			//For each result key:value pair, insert into tempobj
			//key "colname" with value "coltext".
			json_t *value;
			char *colname = strdup(SQL_ColName(stmt, i));
			char *coltext = strdup(SQL_ColText(stmt, i));
			
			if(!colname || !coltext){
				CURRERROR = errCRIT_FUNCT;
				AlertMsg("SQL->JSON ERROR!", coltext);
				safe_free(colname);
				safe_free(coltext);
				return json_array();
			}
			
			if(strndef(coltext)){
				value = json_null();
			} else {
				//AlertMsg(coltext, "DEBUG coltext");
				value = json_string(coltext);
			}
			
			if(value == NULL){
				CURRERROR = errCRIT_FUNCT;
				AlertMsg("There was an error fetching data from\n"
					"the mod database. (Possible memory corruption!)\n"
					"The application will now quit to prevent data\n"
					"corruption. PLEASE contact the developer!",
					"SQL->JSON ERROR!");
				safe_free(colname);
				safe_free(coltext);
				return json_array();
			}
			 
			json_object_set_new(tempobj, colname, value);
			
			safe_free(colname);
			safe_free(coltext);
		}
		//Add object to array
		json_array_append_new(result, tempobj);
		
		errorNo = sqlite3_step(stmt);
        };
	
	if(errorNo != SQLITE_DONE){
		CURRERROR = errCRIT_DBASE;
		result = json_array();
	}

	{
		//DEBUG: Dump JSON text
		//char *msg = json_dumps(result, JSON_INDENT(4) | JSON_ENSURE_ASCII);
	//	AlertMsg(msg, "DEBUG");

	}
	
	//End array
	sqlite3_reset(stmt);
	
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_GetNum
 *  Description:  Returns the first item from an SQL query as a number
 *                (ex: the value of a COUNT statement)
 * =====================================================================================
 */
int SQL_GetNum(sqlite3_stmt *stmt)
{
	int errorNo, result = -1;
	CURRERROR = errNOERR;
	
        errorNo = sqlite3_step(stmt);
        if (errorNo == SQLITE_ROW) {
		result = sqlite3_column_int(stmt, 0);
        } else if (errorNo == SQLITE_DONE){
		result = -1;
	} else {
		CURRERROR = errCRIT_DBASE;
		result = -1;
	}
	
	sqlite3_reset(stmt);
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_GetStr
 *  Description:  Returns the first item from an SQL query as a string
 *                (ex: the value of a SELECT statement)
 * =====================================================================================
 */
char * SQL_GetStr(sqlite3_stmt *stmt)
{
	int errorNo;
	char *result = NULL;
	CURRERROR = errNOERR;
	
        errorNo = sqlite3_step(stmt);
        if (errorNo == SQLITE_ROW) {
            result = strdup(SQL_ColText(stmt, 0));
        } else if (errorNo == SQLITE_DONE){
			return NULL;
	} else {
		CURRERROR = errCRIT_DBASE;
	}
	
	sqlite3_reset(stmt);
	return result;
}

unsigned char * SQL_GetBlob(sqlite3_stmt *stmt, int *noBytes)
{
    int errorNo;
	unsigned char *result = NULL;
    const char *result_tmp = NULL;
	CURRERROR = errNOERR;
	
        errorNo = sqlite3_step(stmt);
        if (errorNo == SQLITE_ROW) {
            result_tmp = sqlite3_column_blob(stmt, 0);
            sqlite3_reset(stmt);
            *noBytes = sqlite3_column_bytes(stmt, 0);
            
            // Make copy
            result = malloc(*noBytes);
            memcpy(result, result_tmp, *noBytes);
            
        } else if (errorNo == SQLITE_DONE){
			return NULL;
	} else {
		CURRERROR = errCRIT_DBASE;
	}
	
	sqlite3_reset(stmt);
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  SQL_HandleErrors
 *  Description:  In the event of an error, handle and attempt to recover from it.
 * =====================================================================================
 */
int SQL_HandleErrors(const char *filename, int lineno, int SQLResult)
{
	char *message = NULL;
    
    if(errno == ENOENT){
        // ENOENT: No file or directory
        // For some reason SQLite loves to set this. This is bad.
        errno = 0;
    }
	
	if(
		SQLResult == SQLITE_OK ||
		SQLResult == SQLITE_DONE ||
		SQLResult == SQLITE_ROW
	){
		return 0;
	}

	if(SQLResult == 5){	// Mostly for people doing debugging
		AlertMsg(
			"This database is open in another application.\n"
			"Close all applications using this database and try again.",
			"SQLite Error!"
		);
		return -1;
	}

	asprintf(&message,
		"Internal Database Error!\n"
		"Error code %d\n"
		"File %s\n"
		"Line %d\n"
		, SQLResult, filename, lineno);
	AlertMsg(message, "SQLite Error!");
	safe_free(message);
	return -1;
}
