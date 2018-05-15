#include "seq/util.h"
#include "seq/revcomp.h"

using namespace seq;

RevComp::RevComp() : OpStage("revcomp", &util::revcomp)
{
}

RevComp& RevComp::make()
{
	return *new RevComp();
}
