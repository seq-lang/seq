#ifndef SEQ_STANDALONETEST_H
#define SEQ_STANDALONETEST_H

#include "../testhelp.h"

TEST(StandaloneTest, StandaloneTest)
{
	SeqModule& s = parse(TEST_DIR "/data/seq/test.seq");
	EXPECT_NO_THROW(s.execute({DEFAULT_TEST_INPUT_MULTI}, true));
}

#endif /* SEQ_STANDALONETEST_H */
