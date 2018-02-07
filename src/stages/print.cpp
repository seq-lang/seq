#include "util.h"
#include "print.h"

using namespace seq;

Print::Print() : Op("print", &util::print)
{
}

Print& Print::make()
{
	return *new Print();
}
