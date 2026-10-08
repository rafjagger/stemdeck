#include <gtest/gtest.h>

#include "SyncLabels.h"

#include <string>
#include <vector>

TEST (SyncLabels, TheSourceButtonNamesTheNetworkOrTheDeck)
{
	EXPECT_EQ (syncLabels::sourceButton (true), "SYNC: LINK");
	EXPECT_EQ (syncLabels::sourceButton (false), "SYNC: DECK");
}

TEST (SyncLabels, APlayerIsCalledAPlayer)
{
	EXPECT_EQ (syncLabels::player (2), "Player 2");
}

TEST (SyncLabels, TheStatusFollowsAPlayer)
{
	EXPECT_EQ (syncLabels::following (128.0, 2), "LINK 128.0 \xc2\xb7 Player 2");
}

TEST (SyncLabels, ASilentMasterIsHeld)
{
	EXPECT_EQ (syncLabels::following (127.96, 0), "LINK 128.0 \xc2\xb7 held");
}

TEST (SyncLabels, TheOtherStates)
{
	EXPECT_EQ (syncLabels::nothingHeard(), "LINK \xe2\x80\x93");
	EXPECT_EQ (syncLabels::noMaster(), "LINK: no master \xc2\xb7 follow");
	EXPECT_EQ (syncLabels::error ("port busy"), "LINK: port busy");
	EXPECT_EQ (syncLabels::sendingMaster (1), "LINK master: B");
}

// stemdeck#4: the UI refers to the protocol it works with, never to a
// maker or its product line.
TEST (SyncLabels, NoLabelNamesABrand)
{
	const std::vector<std::string> labels {
		syncLabels::sourceButton (true), syncLabels::sourceButton (false),
		syncLabels::player (1), syncLabels::following (120.0, 1), syncLabels::following (120.0, 0),
		syncLabels::nothingHeard(), syncLabels::noMaster(), syncLabels::error ("x"),
		syncLabels::sendingMaster (0), syncLabels::masterTooltip(), syncLabels::gridTooltip()
	};
	for (const auto& label : labels)
		for (const auto* brand : { "PIO", "Pioneer", "CDJ", "XDJ", "rekordbox" })
			EXPECT_EQ (label.find (brand), std::string::npos) << label;
}
