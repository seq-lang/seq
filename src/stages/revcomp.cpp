#include "seq/util.h"
#include "seq/revcomp.h"

using namespace seq;

RevComp::RevComp() : Op("revcomp", &util::revcomp)
{
}

RevComp& RevComp::make()
{
	return *new RevComp();
}
