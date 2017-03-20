// Tests if streq macro works at all

#include "../../includes.h"
#include "../../funcproto.h"

int Test_strieq()
{
	const char *str1 = "TEST";
    const char *str2 = "test";

    return strieq(str1, str2);
}
