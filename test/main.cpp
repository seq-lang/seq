#include "testhelp.h"

#include "core/bigtest.h"
#include "stages/calltest.h"
#include "stages/chunktest.h"
#include "stages/collecttest.h"
#include "stages/copytest.h"
#include "stages/counttest.h"
#include "stages/filtertest.h"
#include "stages/lentest.h"
#include "stages/memtest.h"

int main(int argc, char *argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
