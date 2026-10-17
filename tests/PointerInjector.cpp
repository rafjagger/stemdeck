#include <gtest/gtest.h>

#include "PointerInjector.h"

#include <dlfcn.h>

#include <cstdlib>
#include <vector>

// The injector needs a real window, hence a real X server: these tests run
// under xvfb-run (tests/CMakeLists.txt) and skip only when no display exists.

namespace
{
	struct Seen
	{
		juce::MouseInputSource::InputSourceType type;
		int index;
		juce::Point<float> where;
		bool down;
	};

	class Catcher : public juce::Component
	{
	public:
		std::vector<Seen> seen;
		void mouseDown (const juce::MouseEvent& e) override { note (e, true); }
		void mouseDrag (const juce::MouseEvent& e) override { note (e, true); }
		void mouseUp (const juce::MouseEvent& e) override { note (e, false); }

	private:
		void note (const juce::MouseEvent& e, bool down)
		{
			seen.push_back ({ e.source.getType(), e.source.getIndex(), e.position, down });
		}
	};

	// A bare Xvfb has no window manager, so the atoms JUCE only looks up
	// (WM_PROTOCOLS, _NET_WM_WINDOW_TYPE, ...) do not exist, and its first
	// window dies with BadAtom. Creating them, as a window manager would,
	// is what makes the bare server usable. Atoms live as long as one client is connected.
	void internWindowManagerAtoms()
	{
		void* x11 = dlopen ("libX11.so.6", RTLD_NOW);
		if (x11 == nullptr)
			return;
		using Open = void* (*) (const char*);
		using Intern = unsigned long (*) (void*, const char*, int);
		const auto open = reinterpret_cast<Open> (dlsym (x11, "XOpenDisplay"));
		const auto intern = reinterpret_cast<Intern> (dlsym (x11, "XInternAtom"));
		if (open == nullptr || intern == nullptr)
			return;
		if (auto* display = open (nullptr))
		{
			for (const char* name : { "WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW", "_NET_WM_PING",
								 "_NET_WM_WINDOW_TYPE", "_NET_WM_STATE" })
				intern (display, name, 0);
			// Kept open: Xvfb resets, atoms included, when its last client leaves.
			static void* keepAlive = display;
			(void) keepAlive;
		}
	}

	bool hasDisplay() { return std::getenv ("DISPLAY") != nullptr && *std::getenv ("DISPLAY") != 0; }

	struct PointerInjector : ::testing::Test
	{
		void SetUp() override
		{
			if (! hasDisplay())
				GTEST_SKIP() << "no X display";
			internWindowManagerAtoms();
			init = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
			catcher.setSize (200, 200);
			catcher.addToDesktop (0);
			catcher.setVisible (true);
			catcher.setBounds (100, 100, 200, 200);
		}
		void TearDown() override { catcher.removeFromDesktop(); init.reset(); }

		// The share of the screen that lands on the window's local point.
		juce::Point<float> shareOf (juce::Point<float> local)
		{
			const auto window = catcher.getPeer()->getBounds().toFloat();
			const auto screen = juce::Desktop::getInstance().getDisplays().getDisplayForRect (window.toNearestInt())->logicalBounds.toFloat();
			const auto point = window.getPosition() + local;
			return { (point.x - screen.getX()) / screen.getWidth(), (point.y - screen.getY()) / screen.getHeight() };
		}

		std::unique_ptr<juce::ScopedJuceInitialiser_GUI> init;
		Catcher catcher;
	};
}

TEST (RemotePointer, ScreenToWindow)
{
	const juce::Rectangle<float> screen { 0, 0, 768, 1024 };
	const auto whole = remote::screenToWindow ({ .5f, .5f }, screen, screen);
	ASSERT_TRUE (whole.has_value());
	EXPECT_FLOAT_EQ (whole->x, 384.f);
	EXPECT_FLOAT_EQ (whole->y, 512.f);

	EXPECT_FALSE (remote::screenToWindow ({ .5f, .5f }, screen, { 100, 0, 200, 200 }).has_value());

	const auto inside = remote::screenToWindow ({ .2f, .1f }, screen, { 100, 0, 400, 400 });
	ASSERT_TRUE (inside.has_value());
	EXPECT_NEAR (inside->x, 53.6f, 1e-3f);
	EXPECT_NEAR (inside->y, 102.4f, 1e-3f);
}

TEST_F (PointerInjector, ArrivesAsTouchNine)
{
	ASSERT_NE (catcher.getPeer(), nullptr);
	const auto share = shareOf ({ 50.f, 60.f });
	remote::injectPointer (catcher, { remote::PointerEvent::Phase::down, share.x, share.y }, 1000);
	remote::injectPointer (catcher, { remote::PointerEvent::Phase::up, share.x, share.y }, 1010);
	// The events come by the message queue's own route; let it run.
	juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

	ASSERT_FALSE (catcher.seen.empty());
	for (const auto& s : catcher.seen)
	{
		EXPECT_EQ (s.type, juce::MouseInputSource::InputSourceType::touch);
		EXPECT_EQ (s.index, remote::remoteTouchIndex);
	}
	EXPECT_NEAR (catcher.seen.front().where.x, 50.f, 1.f);
	EXPECT_NEAR (catcher.seen.front().where.y, 60.f, 1.f);
	EXPECT_TRUE (catcher.seen.front().down);
	EXPECT_FALSE (catcher.seen.back().down);
}

TEST_F (PointerInjector, AnInjectOutsideTheWindowIsIgnored)
{
	ASSERT_NE (catcher.getPeer(), nullptr);
	const auto share = shareOf ({ 500.f, 60.f });
	remote::injectPointer (catcher, { remote::PointerEvent::Phase::down, share.x, share.y }, 1000);
	juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
	EXPECT_TRUE (catcher.seen.empty());
}

TEST_F (PointerInjector, AnUpOutsideTheWindowStillReleasesTheTouch)
{
	ASSERT_NE (catcher.getPeer(), nullptr);
	const auto inside = shareOf ({ 50.f, 60.f });
	const auto outside = shareOf ({ 500.f, 60.f });
	remote::injectPointer (catcher, { remote::PointerEvent::Phase::down, inside.x, inside.y }, 1000);
	remote::injectPointer (catcher, { remote::PointerEvent::Phase::move, outside.x, outside.y }, 1005);
	remote::injectPointer (catcher, { remote::PointerEvent::Phase::up, outside.x, outside.y }, 1010);
	juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

	ASSERT_GE (catcher.seen.size(), 2u);
	const auto& last = catcher.seen.back();
	EXPECT_FALSE (last.down);
	EXPECT_EQ (last.type, juce::MouseInputSource::InputSourceType::touch);
	EXPECT_EQ (last.index, remote::remoteTouchIndex);
	// Clamped to the window's right edge, same row.
	EXPECT_NEAR (last.where.x, 200.f, 1.5f);
	EXPECT_NEAR (last.where.y, 60.f, 1.f);

	// The injected source is up now: a stray move changes nothing.
	const auto count = catcher.seen.size();
	remote::injectPointer (catcher, { remote::PointerEvent::Phase::move, inside.x, inside.y }, 1020);
	juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
	EXPECT_EQ (catcher.seen.size(), count);
}

TEST_F (PointerInjector, ADownOutsideTheWindowStartsNothing)
{
	ASSERT_NE (catcher.getPeer(), nullptr);
	const auto outside = shareOf ({ 500.f, 60.f });
	const auto inside = shareOf ({ 50.f, 60.f });
	remote::injectPointer (catcher, { remote::PointerEvent::Phase::down, outside.x, outside.y }, 1000);
	remote::injectPointer (catcher, { remote::PointerEvent::Phase::move, inside.x, inside.y }, 1005);
	remote::injectPointer (catcher, { remote::PointerEvent::Phase::up, inside.x, inside.y }, 1010);
	juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
	EXPECT_TRUE (catcher.seen.empty());
}
