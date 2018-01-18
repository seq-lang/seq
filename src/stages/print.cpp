#include "../util.h"
#include "print.h"

using namespace seq;

Print::Print() : Op("dump", &util::dump)
{
}

Print& Print::make()
{
	return *new Print();
}
