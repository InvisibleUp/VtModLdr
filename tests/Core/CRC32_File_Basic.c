// Tests if CRC32_File function works at all

#include "../../includes.h"
#include "../../funcproto.h"

int Test_CRC32_File_Basic()
{
	return (Proto_Checksum("test/CRC32_File_Basic/test.txt", 0xD87F7E0C, TRUE));
}
