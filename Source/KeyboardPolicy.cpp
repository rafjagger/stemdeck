#include "KeyboardPolicy.h"

KeyboardPolicy::KeyboardPolicy (bool onboardAvailable) : available (onboardAvailable) {}

KeyboardPolicy::Command KeyboardPolicy::focusChanged (bool textFieldHasFocus)
{
	if (! available)
		return Command::none;

	const auto hadTextFocus = textFocus;
	textFocus = textFieldHasFocus;

	if (textFieldHasFocus)
	{
		hidePending = false;
		return Command::show;
	}

	// Focus moving between things that were never text fields: a keyboard
	// opened with KEYS is the hand's business, not the focus's.
	if (! hadTextFocus)
		return Command::none;

	hidePending = true;
	return Command::hideSoon;
}

KeyboardPolicy::Command KeyboardPolicy::hideDue()
{
	const auto due = available && hidePending && ! textFocus;
	hidePending = false;
	return due ? Command::hide : Command::none;
}

KeyboardPolicy::Command KeyboardPolicy::keysPressed()
{
	if (! available)
		return Command::hint;

	hidePending = false;
	return Command::toggle;
}

void KeyboardPolicy::setAvailable (bool onboardAvailable)
{
	available = onboardAvailable;

	if (! available)
		hidePending = false;
}
