#include "../../includes.h"
#include "../../funcproto.h"

int Test_Eq_Parse_Int_ConstHex()
{
	return (Eq_Parse_Int("0x42", NULL, FALSE) == 0x42) == TRUE;
}