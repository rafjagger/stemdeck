#include <gtest/gtest.h>

#include "TruthKeeper.h"

// StemDeck takes its truth from Core like the desk (spec truth-from-core,
// step 3): it starts on the cache, hears /core/here, fetches a fingerprint it
// does not have, verifies, stores and restarts. The desk's review taught the
// cases below: no restart on its own fingerprint, none with an override.

TEST (KeeperDecide, TheSameFingerprintNeedsNoFetch)
{
	EXPECT_FALSE (truthkeeper::needsFetch (std::string (64, 'a'), std::string (64, 'a')));
	EXPECT_TRUE (truthkeeper::needsFetch (std::string (64, 'b'), std::string (64, 'a')));
}

TEST (KeeperDecide, AnOverrideIsNotFollowed)
{
	EXPECT_TRUE (truthkeeper::followsCore (nullptr));
	EXPECT_TRUE (truthkeeper::followsCore (""));
	EXPECT_FALSE (truthkeeper::followsCore ("/x.json"));
}

TEST (KeeperVerify, BodyHeaderAndAnnouncementMustAgree)
{
	const std::string h (64, 'c');
	EXPECT_TRUE (truthkeeper::verified (h, h, h));
	EXPECT_FALSE (truthkeeper::verified (h, h, std::string (64, 'd')));
	EXPECT_FALSE (truthkeeper::verified (h, std::string (64, 'd'), h));
	EXPECT_FALSE (truthkeeper::verified ("", "", ""));
}

TEST (KeeperStart, OverrideThenCacheThenPackage)
{
	EXPECT_EQ (truthkeeper::startPath ("/o.json", "/c.json", true, "/p.json"), "/o.json");
	EXPECT_EQ (truthkeeper::startPath (nullptr, "/c.json", true, "/p.json"), "/c.json");
	EXPECT_EQ (truthkeeper::startPath ("", "/c.json", true, "/p.json"), "/c.json");
}

TEST (KeeperStart, AnUnparsableCacheIsSkipped)
{
	EXPECT_EQ (truthkeeper::startPath (nullptr, "/c.json", false, "/p.json"), "/p.json");
}

TEST (KeeperStart, TheCacheIsInHome)
{
	EXPECT_EQ (truthkeeper::cachePath ("/home/aaa"), "/home/aaa/.cache/a3/a3-osc.json");
}

// Step 4 (radla): Core is named in a file, and polled.
TEST (KeeperPoll, CoreIsNamedInTheConfig)
{
	EXPECT_EQ (truthkeeper::corePath ("/home/u"), "/home/u/.config/a3/core");
}

TEST (KeeperPoll, TheUrlIsCoresTruth)
{
	EXPECT_EQ (truthkeeper::pollUrl ("http://h:9080\n"), "http://h:9080/api/truth");
	EXPECT_EQ (truthkeeper::pollUrl ("  http://h:9080/  "), "http://h:9080/api/truth");
	EXPECT_EQ (truthkeeper::pollUrl ("http://h:9080/api/truth"), "http://h:9080/api/truth");
}

TEST (KeeperPoll, ABlankFileIsNotSet)
{
	EXPECT_EQ (truthkeeper::pollUrl (""), "");
	EXPECT_EQ (truthkeeper::pollUrl (" \n\t"), "");
}

TEST (KeeperPoll, EveryThirtySeconds)
{
	EXPECT_EQ (truthkeeper::pollSeconds, 30);
}

// A poll has no announcement: the header stands in for it, so the body must
// still hash to it -- and a header equal to `own` asks for nothing.
TEST (KeeperPoll, TheHeaderStandsInForTheAnnouncement)
{
	const std::string h (64, 'e');
	EXPECT_EQ (truthkeeper::announcedOr ("", h), h);
	EXPECT_EQ (truthkeeper::announcedOr (std::string (64, 'f'), h), std::string (64, 'f'));
	EXPECT_FALSE (truthkeeper::needsFetch (truthkeeper::announcedOr ("", h), h));
}
