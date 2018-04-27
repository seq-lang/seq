#include "testhelp.h"

#include "core/bigtest.h"
#include "core/branchtest.h"
#include "stages/calltest.h"
#include "stages/chunktest.h"
#include "stages/collecttest.h"
#include "stages/copytest.h"
#include "stages/counttest.h"
#include "stages/filtertest.h"
#include "stages/lentest.h"
#include "stages/makerectest.h"
#include "stages/memtest.h"
#include "stages/serializetest.h"

int main(int argc, char *argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
