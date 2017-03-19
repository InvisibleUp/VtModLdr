#include "../../includes.h"
#include "../../funcproto.h"

int Test_Eq_Parse_Int_ConstDec()
{
	return (Eq_Parse_Int("42", NULL, FALSE) == 42);
}