#pragma once

#include <JuceHeader.h>
#include "KeyboardPolicy.h"

// Runs KeyboardPolicy's commands against onboard, the on-screen keyboard: a
// text field taking focus shows it, focus leaving the text fields hides it a
// moment later, KEYS toggles it.
//
// Onboard is its own process, steered over D-Bus with dbus-send. The calls run
// on a worker thread, never on the message thread; when onboard does not
// answer it is started once, detached, and asked again. Without an onboard
// binary on the PATH nothing is started and KEYS only gives a hint.
class OnScreenKeyboard  : private juce::FocusChangeListener,
						  private juce::Timer
{
public:
	OnScreenKeyboard();
	~OnScreenKeyboard() override;

	void keysPressed();
	bool isAvailable() const { return policy.isAvailable(); }

	// A short line for the status label: why KEYS did nothing.
	std::function<void (const juce::String&)> onHint;

private:
	class Worker;

	void globalFocusChanged (juce::Component* focusedComponent) override;
	void timerCallback() override;
	void run (KeyboardPolicy::Command command);
	void onboardFailed (const juce::String& why, bool stillAvailable);

	KeyboardPolicy policy;
	std::unique_ptr<Worker> worker;

	JUCE_DECLARE_WEAK_REFERENCEABLE (OnScreenKeyboard)
	JUCE_DECLARE_NON_COPYABLE (OnScreenKeyboard)
};
