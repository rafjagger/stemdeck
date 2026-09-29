#pragma once

#include <JuceHeader.h>
#include "StemDeckPlayer.h"

// CDJ-style jog wheel.
//  - Top (platter): in vinyl mode, touching holds the record and turning it
//    scratches, forwards or backwards; releasing lets the deck carry on.
//    Without vinyl mode the top acts like the ring.
//  - Ring (outer edge): while playing, turning bends the pitch (nudge);
//    while stopped, it searches through the track.
//  - Mouse wheel: nudge while playing, fine search while stopped.
// The platter turns with the playhead at 33 1/3 rpm.
class JogWheel : public juce::Component
{
public:
	JogWheel (StemDeckPlayer& player, int deckIndex);

	void setVinylMode (bool shouldUseVinylMode) { vinylMode = shouldUseVinylMode; }
	void refresh(); // called at UI rate: eases pitch bend back and redraws

	void paint (juce::Graphics& g) override;
	void mouseDown (const juce::MouseEvent& e) override;
	void mouseDrag (const juce::MouseEvent& e) override;
	void mouseUp (const juce::MouseEvent& e) override;
	void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

	static constexpr double secondsPerRevolution = 1.8; // 33 1/3 rpm

private:
	enum class Touch { none, platter, ring };

	float angleAt (juce::Point<float> p) const;
	juce::Point<float> centre() const { return getLocalBounds().toFloat().getCentre(); }
	float radius() const { return (float) juce::jmin (getWidth(), getHeight()) * 0.5f - 2.0f; }

	StemDeckPlayer& player;
	const int deckIndex;
	bool vinylMode = true;

	Touch touch = Touch::none;
	float lastAngle = 0.0f;
	double lastMoveTime = 0.0;
	double bend = 1.0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JogWheel)
};
