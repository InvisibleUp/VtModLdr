/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "includes.h"              // LOCAL: All includes
#include "funcproto.h"             // LOCAL: Function prototypes and structs
#include "errormsgs.h"             // LOCAL: Canned error messages

///Jansson helper functions
///////////////////////////

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_Load
 *  Description:  Safely gives a json_t pointer to a JSON file
 *                given a filename. Only works with object roots.
 * =====================================================================================
 */

json_t * JSON_Load(const char *fpath)
{
	/// Load the file
	// Returns NULL on failure and root element on success
	json_error_t error;
	json_t *root = json_load_file(fpath, 0, &error);
	if(!root){
		char *buf;
		asprintf (&buf,
			"An error has occurred loading %s.\n\n"
			//"Contact the mod developer with the following info:\n"
			"line %d:%d - %s", fpath, error.line,
			error.column, error.text);
		AlertMsg (buf, "Invalid JSON");
		safe_free(buf);
		return NULL;
	}
	if(!json_is_object(root)){
		AlertMsg ("An error has occurred loading the file.\n\n"
			//"Contact the mod developer with the following info:\n"
			"The root element of the JSON is not an object.",
			"Invalid JSON");
		json_decref(root);
		return NULL;
	}
	return root;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_GetuInt
 *  Description:  Gives an unsigned integer given a JSON tree and the name of a tag
 * =====================================================================================
 */
unsigned long JSON_GetuInt(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	
	if (json_is_string(object)){
		char *temp = JSON_GetStr(root, name);
		unsigned long result = strtoul(temp, NULL, 0);
		safe_free(temp);
		return result;
	} else if (json_is_number(object)){
		return (unsigned long)json_integer_value(object);
	} else {
		return 0;
	}
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  JSON_GetInt
*  Description:  Gives a signed integer given a JSON tree and the name of a tag
* =====================================================================================
*/
signed long JSON_GetInt(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);

	if (json_is_string(object)) {
		char *temp = JSON_GetStr(root, name);
		long result = strtoul(temp, NULL, 0);
		safe_free(temp);
		return result;
	}
	else if (json_is_number(object)) {
		return (signed long)json_integer_value(object);
	}
	else {
		return 0;
	}
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_GetDouble
 *  Description:  Gives a double given a JSON tree and the name of a tag
 * =====================================================================================
 */
double JSON_GetDouble(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL) {return 0;}
	if (json_is_string(object)) {
		char *temp = JSON_GetStr(root, name);
		double result = strtod(temp, NULL);
		safe_free(temp);
		return result;
	}
	return json_number_value(object);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_GetStr
 *  Description:  Gives a string given a JSON tree and the name of a tag
 * =====================================================================================
 */
char * JSON_GetStr(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL){
		//return strdup("");
		return NULL;
	}
	if (json_is_integer(object)){
		int outint = JSON_GetInt(root, name);
		char *out = NULL;
		asprintf(&out, "%d", outint);
		return out;
	} else if (json_is_real(object)){
		double outint = JSON_GetDouble(root, name);
		char *out = NULL;
		asprintf(&out, "%f", outint);
		return out;
	}
	if (!json_is_string(object)){
		//return strdup("");
		return NULL;
	}
	return strdup(json_string_value(object));
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_GetStrLen
 *  Description:  Gives the length of a string given a JSON tree and the name of a tag
 * =====================================================================================
 */
int JSON_GetStrLen(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL) {
		return 0;
	}
	if (json_is_integer(object)) {
		return ItoaLen(JSON_GetuInt(root, name));
	} else if (json_is_real(object)) {
		return FtoaLen(JSON_GetDouble(root, name));
	}
	if (!json_is_string(object)){
		return 0;
	}
	return json_string_length(object);
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  JSON_FindArrElemByKeyValue
*  Description:  Given an array of objects, finds the array element that contains
*                the given key-value pair. Returns NULL if not found or not an array.
* =====================================================================================
*/
json_t * JSON_FindArrElemByKeyValue(json_t *root, const char *key, json_t *value)
{
	size_t i;
	json_t *row;

	if (!json_is_array(root)) {
		return NULL;
	}

	json_array_foreach(root, i, row) {
		json_t *result = json_object_get(row, key);
		if (json_equal(result, value)) {
			return row;
		}
	}

	return NULL;
}