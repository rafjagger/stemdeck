#include "PointerInjector.h"

namespace remote
{
	std::optional<juce::Point<float>> screenToWindow (juce::Point<float> share, juce::Rectangle<float> screen,
													  juce::Rectangle<float> window)
	{
		const juce::Point<float> onScreen { screen.getX() + share.x * screen.getWidth(),
											screen.getY() + share.y * screen.getHeight() };
		if (! window.contains (onScreen))
			return std::nullopt;
		return onScreen - window.getPosition();
	}

	void injectPointer (juce::Component& topLevel, const Inject& inject, juce::int64 timeMs)
	{
		auto* peer = topLevel.getPeer();
		if (peer == nullptr)
			return;

		const auto window = peer->getBounds();
		const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (window);
		if (display == nullptr)
			return;

		const auto local = screenToWindow ({ inject.x, inject.y }, display->logicalBounds.toFloat(), window.toFloat());
		if (! local)
			return;

		const auto mods = inject.phase == PointerEvent::Phase::up
							  ? juce::ModifierKeys()
							  : juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
		peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::touch, *local, mods, 1.0f, 0.0f, timeMs,
								{}, remoteTouchIndex);
	}
}
