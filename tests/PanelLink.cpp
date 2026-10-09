#include <gtest/gtest.h>

#include "PanelLink.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Looking for a panel that is not there costs a sysfs walk and maybe an open:
// at once on start, then every two seconds, never on every poll.
TEST (PanelLink, LooksAtOnceThenEveryTwoSeconds)
{
	panel::Reconnect link;
	EXPECT_TRUE (link.shouldLook (10'000));
	EXPECT_FALSE (link.shouldLook (10'500));
	EXPECT_FALSE (link.shouldLook (11'999));
	EXPECT_TRUE (link.shouldLook (12'000));
	EXPECT_FALSE (link.shouldLook (12'001));
}

// Eight quiet polls -- two seconds at the read timeout -- and the port is let
// go; one frame in between starts the count over.
TEST (PanelLink, QuietPollsLetThePortGo)
{
	panel::Reconnect link;
	for (int i = 1; i < panel::Reconnect::quietPollsBeforeClosing; ++i)
		EXPECT_FALSE (link.noteQuietPoll()) << i;
	link.noteFrame();
	for (int i = 1; i < panel::Reconnect::quietPollsBeforeClosing; ++i)
		EXPECT_FALSE (link.noteQuietPoll()) << i;
	EXPECT_TRUE (link.noteQuietPoll());
	EXPECT_FALSE (link.noteQuietPoll()) << "counted afresh";
}

namespace
{
	struct FakeSysfs
	{
		fs::path root = fs::temp_directory_path() / ("stemdeck-panel-sysfs-" + std::to_string (::testing::UnitTest::GetInstance()->random_seed()));

		FakeSysfs() { fs::remove_all (root); fs::create_directories (root / "class/tty"); }
		~FakeSysfs() { fs::remove_all (root); }

		static void write (const fs::path& file, const std::string& text) { std::ofstream (file) << text << "\n"; }

		// A USB device with an ACM interface carrying `tty`, as the kernel lays it out.
		void addAcm (const std::string& tty, const std::string& usb, const std::string& vendor, const std::string& product)
		{
			const auto device = root / "devices/pci0000:00/usb1" / usb;
			const auto ttyDir = device / (usb + ":1.0") / "tty" / tty;
			fs::create_directories (ttyDir);
			write (device / "idVendor", vendor);
			write (device / "idProduct", product);
			fs::create_directory_symlink ("../..", ttyDir / "device");
			fs::create_directory_symlink (fs::relative (ttyDir, root / "class/tty"), root / "class/tty" / tty);
		}
	};
}

// Only the panel's own bridge (1a86:55d3), never a tty by its number: the
// other ACM device and the platform's serial line are not asked at all.
TEST (PanelLink, FindsThePanelByItsUsbIdOnly)
{
	FakeSysfs sysfs;
	sysfs.addAcm ("ttyACM0", "1-1", "303a", "1001");
	sysfs.addAcm ("ttyACM1", "1-2", "1a86", "55d3");
	sysfs.addAcm ("ttyACM10", "1-4", "1A86", "55D3");
	sysfs.addAcm ("ttyACM2", "1-5", "1a86", "55d3");
	sysfs.addAcm ("ttyACM3", "1-6", "303a", "55d3");
	fs::create_directories (sysfs.root / "class/tty/ttyS0");

	EXPECT_EQ (panel::findPorts (sysfs.root / "class/tty"),
			   (std::vector<std::string> { "/dev/ttyACM1", "/dev/ttyACM2", "/dev/ttyACM10" }));
}

TEST (PanelLink, NoSysfsNoPorts)
{
	EXPECT_TRUE (panel::findPorts ("/nonexistent/class/tty").empty());
}
