#include <gtest/gtest.h>

#include "FolderChoice.h"

TEST (FolderChoice, ShowsTheCurrentFolderAtFirst)
{
	FolderChoice choice { "/music/stems" };
	EXPECT_EQ (choice.shown(), "/music/stems");
	EXPECT_FALSE (choice.toApply().has_value());
}

TEST (FolderChoice, APickIsShownButOnlyAppliedByOk)
{
	FolderChoice choice { "/music/stems" };
	choice.picked ("/data/stems");
	EXPECT_EQ (choice.shown(), "/data/stems");
	EXPECT_EQ (choice.toApply(), std::optional<std::string> ("/data/stems"));
}

// The file chooser closed without a choice hands back an empty path.
TEST (FolderChoice, AClosedChooserChangesNothing)
{
	FolderChoice choice { "/music/stems" };
	choice.picked ("/data/stems");
	choice.picked ("");
	EXPECT_EQ (choice.shown(), "/data/stems");
}

TEST (FolderChoice, PickingTheCurrentFolderAgainAppliesNothing)
{
	FolderChoice choice { "/music/stems" };
	choice.picked ("/data/stems");
	choice.picked ("/music/stems");
	EXPECT_FALSE (choice.toApply().has_value());
}
