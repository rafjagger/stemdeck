#include <gtest/gtest.h>

#include "JackFormat.h"

// PipeWire's libjack calls the sample-rate callback from inside
// jack_set_sample_rate_callback, before the block size is known. Preparing the
// source with a block of 0 there made the binaural decoder's convolution
// divide by zero (SIGFPE at start of every -bformat deck).

TEST (JackFormat, ARateWithoutABlockSizeIsNotAFormat)
{
	EXPECT_FALSE (jackformat::complete (0, 48000.0));
	EXPECT_FALSE (jackformat::complete (0, 44100.0));
}

TEST (JackFormat, ABlockSizeWithoutARateIsNotAFormat)
{
	EXPECT_FALSE (jackformat::complete (128, 0.0));
}

TEST (JackFormat, BothKnownIsAFormat)
{
	EXPECT_TRUE (jackformat::complete (128, 44100.0));
	EXPECT_TRUE (jackformat::complete (1024, 48000.0));
}
