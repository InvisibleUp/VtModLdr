/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "includes.h"
#include "funcproto.h"
#include "errormsgs.h"
#include "shims/crc32/crc32.h"

// Yep. This one's using C++. I need that queue.
// (I might, in the future, move everything to C++ 98)
// (There's not much a reason *not* to.)
#include <queue>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <stack>

std::queue<std::string> Eq_Tokenize(const char *input);
std::queue<std::string> Eq_Reorder(std::queue<std::string> input);
BOOL Eq_OpIsFileFunct(const std::string op);
template <typename T> T Eq_Compute_DoOp(T a, T b, std::string op);
template <typename T> T CppStrToNum(std::string const text);
template <typename T> T Eq_Compute(std::queue<std::string> input, const char *ModPath);

// Parse an equation string stored as a C string
int Eq_Parse_Int(const char * eq, const char *ModPath){
	std::queue<std::string> input;
	int result;
	CURRERROR = errNOERR;
	
	result = Eq_Compute<int>(
		Eq_Reorder(
			Eq_Tokenize(eq)
		), ModPath
	);
	
	if(CURRERROR == errNOERR) return result;
	else return 0;
}

// Like above, but unsigned
unsigned int Eq_Parse_uInt(const char * eq, const char *ModPath){
	std::queue<std::string> input;
	unsigned int result;
	CURRERROR = errNOERR;
	
	result = Eq_Compute<unsigned int>(
		Eq_Reorder(
			Eq_Tokenize(eq)
		), ModPath
	);
	
	if(CURRERROR == errNOERR) return result;
	else return 0;
}

// Like above, but for floating-point numbers
double Eq_Parse_Double(const char * eq, const char *ModPath){
	std::queue<std::string> input;
	double result;
	CURRERROR = errNOERR;
	
	result = Eq_Compute<int>(
		Eq_Reorder(
			Eq_Tokenize(eq)
		), ModPath
	);
	
	if(CURRERROR == errNOERR) return result;
	else return 0;
}

// Convert infixed string of operators/numbers to queue
// Almost textbook implementation of strtok()
std::queue<std::string> Eq_Tokenize(const char *input){
	// Convert const char* to std::string
	std::string eq(input);
	std::queue<std::string> output;
	
	// Totally copy/pasted from SO.
	std::istringstream ss(eq);
	std::string s;

	while(std::getline(ss, s, ' ')){
		output.push(s);
	}

	return output;
}

// For map
/*struct cmp_str
{
   bool operator()(const char *a, const char *b)
   {
      return strcmp(a, b) < 0;
   }
};*/

// Use railyard shunting algorithm to convert infix to RPN
std::queue<std::string> Eq_Reorder(std::queue<std::string> input)
{
	std::stack<std::string> op;
	std::queue<std::string> result;
	
	// Set up precedence order (like C)
	// All operators are left-associative
	std::map<std::string, int> order;
	
	// Grouping
	order["("] = 0;
	order[")"] = 0;
	// Unary
	order["~"] = 1; // Bitwise NOT
	order["crc32"] = 1; // Compute CRC32 of data
	order["len"] = 1; // Length of given data in bytes
	order["exists"] = 1; // Var/file exists?
	order["$"] = 1; // Deference variable
	order["@"] = 1; // Load file from game dir
	order["#"] = 1; // Load file from mod dir
	// Multiplication
	order["*"] = 2;
	order["/"] = 2;
	order["%"] = 2;
	// Addition
	order["+"] = 3;
	order["-"] = 3;
	// Bit shifters
	order["<<"] = 4; // LSR
	order[">>"] = 4; // RSB
	// Inequality operators
	order["<"] = 5;
	order[">"] = 5;
	order["<="] = 5;
	order[">="] = 5;
	// Equality operators
	order["=="] = 6;
	order["!="] = 6;
	// Bit operators
	order["&"] = 7; // AND
	order["^"] = 7; // XOR
	order["|"] = 7; // OR
	// Boolearn operators
	order["&&"] = 8;
	order["||"] = 8;
	
	// To possibly add in future:
	// - Boolean comparison operators (... why not now?)
	// - Exponentiation & roots
	
	// For each token...
	while(!input.empty()){
		std::string token = input.front();
		std::map<std::string, int>::iterator it;
		int op1pow;
		
		input.pop();
		it = order.find(token.c_str());

		// If it's a number or identifier (not an operator), 
		// push to output queue.
		// If it's not anything, it'll parse as 0 later, so let it be.
		if(it == order.end()){
			result.push(token);
			continue;
		}

		op1pow = it->second;
		
		// If operator is ")", move every operator up to "("
		// from op queue to result queue and remove "(".
		if(token == ")"){
			while(!op.empty() && op.top() != "("){
				result.push(op.top());
				op.pop();
			}
			
			if(!op.empty() && op.top() == "("){
				op.pop();
			}
			
			// Also if the next operator is a unary, pop that too.
			if(op.empty()){
				continue;
			} else {
				int op2pow;
				std::map<std::string, int>::iterator it;
				it = order.find(op.top());
				op2pow = it->second;
				
				if(op2pow <= 1){
					result.push(op.top());
					op.pop();
				}
				
				continue;
			}
		}
		
		// From here on token must be an operator //
		
		// If operator is <= front of op queue, 
		// move front of op queue to result queue
		if(!op.empty()){
			int op2pow;
			std::map<std::string, int>::iterator it;
			
			it = order.find(op.top());
			op2pow = it->second;
			
			if(op1pow > op2pow){
				result.push(op.top());
				op.pop();
			}
		}
		
		// Push token onto operator queue
		op.push(token);
	}
	
	// Dump remaining operators on the end
	while(!op.empty()){
		result.push(op.top());
		op.pop();
	}
	
	return result;
}

BOOL Eq_OpIsFileFunct(const std::string op){
	return (op == "crc32" || op == "len"); 
}

template <typename T> T Eq_Compute_DoOp(T a, T b, std::string op){
	if(op == "+") return a + b;
	if(op == "-") return a - b;
	if(op == "/") return a / b;
	if(op == "*") return a * b;
	if(op == "%") return a & b;
	
	if(op == "|") return a | b;
	if(op == "&") return a & b;
	if(op == "^") return a ^ b;
	if(op == ">>") return a >> b;
	if(op == "<<") return a << b;

	if(op == "==") return a == b;
	if(op == "<=") return a <= b;
	if(op == ">=") return a >= b;
	if(op == "<") return a < b;
	if(op == ">") return a > b;
	if(op == "!=") return a != b;
	
	if(op == "&&") return a && b;
	if(op == "||") return a || b;

	return 0;
}

// Also StackOverflow somewhere
template <typename T> T CppStrToNum(std::string const text)
{
   T value;
   std::stringstream ss(text);
   ss >> value;
   return value;
}

// https://stackoverflow.com/a/5590404
#define CppNumToStr( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

// Get a variable's value if needed
template <typename T> T Eq_GetArgValue(std::string arg)
{
	if(!Var_Exists(arg.c_str())){
		return CppStrToNum<T>(arg);
	}

	struct VarValue var = Var_GetValue_SQL(arg.c_str());
	T result;
	
	if(var.type == IEEE64 || var.type == IEEE32){
		result = Var_GetDouble(&var);
	} else {
		result = Var_GetInt(&var);
	}

	Var_Destructor(&var);
	return result;
}

BOOL StrInStrArray(const char *array[], int len, const char *item)
{
	int i;
	for(i = 0; i < len; i++){
		if(streq(array[i], item)){
			return TRUE;
		}
	}
	return FALSE;
}

// Computes a function queue in RPN order.
template <typename T> T Eq_Compute(std::queue<std::string> input, const char *ModPath)
{
	const char *ops[] = {
		"*", "/", "%", "+", "-", "<<", ">>", "&", "^", "|", "==",
		"!=", "<", ">", "<=", ">=", "||", "&&"
	};
	std::stack<std::string> ArgStack;
	std::string ModPathNew(ModPath);

	while(!input.empty()){
		std::string token = input.front();
		input.pop();
		
		// Determine type of current token
		
		// Is it a unary operator?
		if(token == "~"){ // There's literally only one.
			T Arg1 = CppStrToNum<T>(ArgStack.top());
			ArgStack.pop();

			Arg1 = ~Arg1;
			ArgStack.push(CppNumToStr(Arg1));
			continue;
		}
		
		// Is it a binary operator?
		if(StrInStrArray( ops, (sizeof(ops)/sizeof(ops[0])), token.c_str() )){
			T Arg1 = Eq_GetArgValue<T>(ArgStack.top());
			ArgStack.pop();
			T Arg2 = Eq_GetArgValue<T>(ArgStack.top());
			ArgStack.pop();
			
			// Compute value.
			ArgStack.push(CppNumToStr(Eq_Compute_DoOp<T>(Arg1, Arg2, token)));
			continue;
		}

		// Must be a number if the only value remaining
		// (Can't continue; will crash)
		if(input.empty()){
			ArgStack.push(token);
			continue;
		}
			
		// Check if the next operator is a dereferencer
		std::string derefOp = input.front();
			
		// Is it a known variable?
		if (derefOp == "$") {
			input.pop(); // Remove operator from stack;

			if(input.empty()){
				// Just get value; stack's empty
				ArgStack.push(CppNumToStr(Eq_GetArgValue<T>(token)));
				continue;
			}

			std::string VarOp = input.front();
			if(VarOp == "exists"){
				// Return if variable exists or not
				input.pop();
				ArgStack.push(CppNumToStr(Var_Exists(token.c_str())));
			} else {
				// Just get value; that's what they want
				ArgStack.push(CppNumToStr(Eq_GetArgValue<T>(token)));
				continue;
			}
		}

		// Check to make sure we have two operators remaining
		if(input.size() <= 1){
			ArgStack.push(token);
			continue;
		}
		
		// Get file path
		std::string FilePath;
		if(derefOp == "@"){ // File in game dir
			FilePath = (CONFIG.CURRDIR + ("/" + token));
		} else if (derefOp == "#") { // Mod dir
			std::string FilePath = (ModPathNew + ("/" + token));
		}

		// File stuff
		if(derefOp == "@" || derefOp == "#"){
			input.pop();

			if(input.empty()){
				// Need additional operator
				// (Maybe I'll support raw values later.)
				CURRERROR = errWNG_MODCFG;
				return 0;
			}

			std::string FileOp = input.front();
			input.pop();
			BOOL Exists = File_Exists(FilePath.c_str(), FALSE, TRUE);
			
			if (FileOp == "exists") {
				ArgStack.push(CppNumToStr(Exists));
				continue;

			} else if (!Exists) {
				// Return 0. File doesn't exist, right?
				ArgStack.push("0");
				continue;
			}
			
			if (FileOp == "crc32") {
				// Yes, you'll get a negative checksum, but
				// if you're just comparing this is fine.
				ArgStack.push(CppNumToStr((T)crc32File(FilePath.c_str())));
			} else if (FileOp == "len") {
				ArgStack.push(CppNumToStr((T)filesize(FilePath.c_str())));
			} else {
				CURRERROR = errWNG_MODCFG;
				return 0;
			}
			continue;
		}
	}

	// We got a final result
	if(ArgStack.size() != 1) {
		// Mismatched input
		AlertMsg("Mismatched input!", "DEBUG");
		CURRERROR = errWNG_MODCFG;
		return 0;
	}
	else {
		return Eq_GetArgValue<T>(ArgStack.top());
	}
}