// Tests if streq macro works at all

#include "../../includes.h"
#include "../../funcproto.h"

int Test_streq()
{
	const char *str1 = "Test";
    const char *str2 = "Test";
    
    return streq(str1, str2);
}
