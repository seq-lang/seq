#include "testhelp.h"

TEST(BigTest, BigTest)
{
	EXPECT_NO_THROW(bigTest());
}

#include "stages/calltest.h"
#include "stages/counttest.h"
#include "stages/lentest.h"

int main(int argc, char *argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
