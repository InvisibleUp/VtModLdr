#pragma once
#include <vector>
#include <string>

// main.cpp
class Compiler;
class Variable;
class Import;
class Function;
class Patch;
class Mod;
std::vector<Patch> operator+(const std::vector<Patch>& a, const std::vector<Function>& b);

// json.cpp
json_t * JSON_Load(const char *fpath);
unsigned long JSON_Value_uLong(json_t *root, const char *name);
signed long JSON_Value_Long(json_t *root, const char *name);
double JSON_Value_Double(json_t *root, const char *name);
std::string JSON_Value_String(json_t *root, const char *name);
json_t * JSON_FindArrElemByKeyValue(json_t *root, const char *key, json_t *value);
