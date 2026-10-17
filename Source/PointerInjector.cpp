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

	std::optional<juce::Point<float>> localPointFor (PointerEvent::Phase phase, bool touchDown, juce::Point<float> share,
													 juce::Rectangle<float> screen, juce::Rectangle<float> window)
	{
		if (phase == PointerEvent::Phase::down)
			return screenToWindow (share, screen, window);
		if (! touchDown)
			return std::nullopt;
		const juce::Point<float> onScreen { screen.getX() + share.x * screen.getWidth(),
											screen.getY() + share.y * screen.getHeight() };
		return window.getConstrainedPoint (onScreen) - window.getPosition();
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

		static bool touchDown = false;
		const auto local = localPointFor (inject.phase, touchDown, { inject.x, inject.y },
										  display->logicalBounds.toFloat(), window.toFloat());
		if (! local)
			return;
		touchDown = inject.phase != PointerEvent::Phase::up;

		const auto mods = inject.phase == PointerEvent::Phase::up
							  ? juce::ModifierKeys()
							  : juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
		peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::touch, *local, mods, 1.0f, 0.0f, timeMs,
								{}, remoteTouchIndex);
	}
}
