#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
#include "jansson.h"
#include "functproto.hpp"

// http://stackoverflow.com/a/5590404
#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

///Jansson helper functions
///////////////////////////

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_Load
 *  Description:  Safely gives a json_t pointer to a JSON file
 *                given a filename.
 * =====================================================================================
 */

json_t * JSON_Load(const char *fpath)
{
	/// Load the file
	// Returns NULL on failure and root element on success
	json_error_t error;
	json_t *root = json_load_file(fpath, 0, &error);
	if(!root){
		std::cerr << "An error has occurred loading " << fpath << ".\n"
            "line " << error.line << ":" << error.column << " - "
            << error.text << "\n\n";
		return NULL;
	}
	return root;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_Value (Unsigned Long)
 *  Description:  Gives an unsigned integer given a JSON tree and the name of a tag
 * =====================================================================================
 */
unsigned long JSON_Value_uLong(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	
	if (json_is_string(object)){
		const char *temp = JSON_Value_String(root, name).c_str();
		unsigned long result = strtoul(temp, NULL, 0);
		return result;
	} else if (json_is_number(object)){
		return (unsigned long)json_integer_value(object);
	} else {
		return 0;
	}
}

/*
* ===  FUNCTION  ======================================================================
*         Name:  JSON_Value (Signed Long)
*  Description:  Gives a signed integer given a JSON tree and the name of a tag
* =====================================================================================
*/
signed long JSON_Value_Long(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);

	if (json_is_string(object)) {
		const char *temp = JSON_Value_String(root, name).c_str();
		long result = strtoul(temp, NULL, 0);
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
 *         Name:  JSON_Value (Double)
 *  Description:  Gives a double given a JSON tree and the name of a tag
 * =====================================================================================
 */
double JSON_Value_Double(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL) {return 0;}
	if (json_is_string(object)) {
		const char *temp = JSON_Value_String(root, name).c_str();
		double result = strtod(temp, NULL);
		return result;
	}
	return json_number_value(object);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  JSON_Value (String)
 *  Description:  Gives a string given a JSON tree and the name of a tag
 * =====================================================================================
 */
std::string JSON_Value_String(json_t *root, const char *name)
{
	json_t *object = json_object_get(root, name);
	if (object == NULL){
		return std::string("");
	}
	if (json_is_integer(object)){
		return SSTR(JSON_Value_Long(root, name));
	} else if (json_is_real(object)){
		return SSTR(JSON_Value_Double(root, name));
	}
	if (!json_is_string(object)){
		return std::string("");
	}
	return std::string(json_string_value(object));
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
