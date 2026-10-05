#pragma once

// When the on-screen keyboard (onboard) shows and hides. Pure: fed the events,
// returns what to do; OnScreenKeyboard runs the commands.
//
// Onboard's own auto-show follows AT-SPI accessibility, which JUCE windows do
// not offer, so StemDeck switches it from its own focus changes.
class KeyboardPolicy
{
public:
	enum class Command
	{
		none,
		show,
		hide,
		toggle,   // KEYS: whatever onboard shows now, the other way round
		hideSoon, // arm the debounce; hideDue() decides when it runs out
		hint      // KEYS without onboard: say why nothing happens
	};

	explicit KeyboardPolicy (bool onboardAvailable);

	Command focusChanged (bool textFieldHasFocus);
	Command hideDue();
	Command keysPressed();

	void setAvailable (bool onboardAvailable);
	bool isAvailable() const { return available; }

private:
	bool available;
	bool textFocus = false;
	bool hidePending = false;
};
