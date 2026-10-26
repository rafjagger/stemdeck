// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "StemSpatialPopup.h"

#include "DeckMotion.h"

#include <cmath>

namespace
{
	constexpr int refreshHz = 30;
	// Shares of the popup's height: four slider rows, the pads, the length row.
	constexpr float sliderRowShare = 0.065f, padRowShare = 0.11f, lengthRowShare = 0.08f, gapShare = 0.015f;
	// A pad's face: armed blinks on the deck's beat between these two, running is the stem's colour.
	constexpr float armedLit = 0.55f, armedDim = 0.2f, armedNoBeat = 0.35f;

	const std::array<const char*, 4> padNames { "ORBIT", "PING", "RISE", "SCAT" };

	void setUp (juce::Slider& slider, juce::Label& label, const juce::String& name, juce::Range<double> range, double step,
				double value, const juce::String& suffix, std::atomic<float>& target, juce::Component& parent)
	{
		slider.setSliderStyle (juce::Slider::LinearHorizontal);
		slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);
		slider.setRange (range.getStart(), range.getEnd(), step);
		slider.setTextValueSuffix (suffix);
		slider.setValue (value, juce::dontSendNotification);
		slider.setMouseClickGrabsKeyboardFocus (false);
		// After the value is set: opening the popup must not write anything.
		slider.onValueChange = [&slider, &target] { target.store ((float) slider.getValue()); };
		label.setText (name, juce::dontSendNotification);
		label.setJustificationType (juce::Justification::centredLeft);
		parent.addAndMakeVisible (label);
		parent.addAndMakeVisible (slider);
	}

	juce::String lengthName (double bars)
	{
		if (bars < 1.0)
			return juce::String::fromUTF8 ("\xc2\xbd BAR");
		return juce::String ((int) bars) + (bars > 1.0 ? " BARS" : " BAR");
	}

	void setUpKey (juce::TextButton& key, const juce::String& text, juce::Component& parent)
	{
		key.setButtonText (text);
		key.setMouseClickGrabsKeyboardFocus (false);
		key.setWantsKeyboardFocus (false);
		parent.addAndMakeVisible (key);
	}
}

StemSpatialPopup::StemSpatialPopup (DeckSpatial::Controls& c, int stem, DeckMotion* deckMotion)
	: controls (c), stemIndex ((size_t) juce::jlimit (0, DeckSpatial::stems - 1, stem)), motion (deckMotion),
	  stemColour (findColour (juce::Slider::rotarySliderFillColourId))
{
	const auto s = stemIndex;
	setUp (threeDSlider, threeDLabel, "3D", { 0.0, 1.0 }, 0.01, controls.threeD[s].load(), {}, controls.threeD[s], *this);
	setUp (azimuthSlider, azimuthLabel, "AZ", { -180.0, 180.0 }, 1.0, controls.azimuth[s].load(), " deg", controls.azimuth[s], *this);
	setUp (elevationSlider, elevationLabel, "EL", { -90.0, 90.0 }, 1.0, controls.elevation[s].load(), " deg", controls.elevation[s], *this);
	setUp (widthSlider, widthLabel, "WIDTH", { 0.0, 360.0 }, 1.0, controls.width[s].load(), " deg", controls.width[s], *this);

	// Double-click returns to the value a fresh stem has.
	threeDSlider.setDoubleClickReturnValue (true, 0.0);
	azimuthSlider.setDoubleClickReturnValue (true, 0.0);
	elevationSlider.setDoubleClickReturnValue (true, 0.0);
	widthSlider.setDoubleClickReturnValue (true, spatial::Pose{}.width);

	addAndMakeVisible (stemViewer);
	stemViewer.onHomeChanged = [this] (spatial::Pose pose) { homeDragged (pose); };
	for (auto* slider : { &azimuthSlider, &elevationSlider, &widthSlider })
		slider->onValueChange = [this, write = slider->onValueChange]
		{
			write();
			refresh();   // the viewer follows the sliders
		};

	if (motion != nullptr)
	{
		for (size_t p = 0; p < pads.size(); ++p)
		{
			setUpKey (pads[p], padNames[p], *this);
			pads[p].onClick = [this, p] { pressPad ((motionlite::Shape) p); };
		}
		setUpKey (shorter, juce::String::fromUTF8 ("\xe2\x97\x82"), *this);
		setUpKey (longer, juce::String::fromUTF8 ("\xe2\x96\xb8"), *this);
		setUpKey (shiftKey, "SHIFT", *this);
		shiftKey.setClickingTogglesState (true);
		shorter.onClick = [this] { motion->lengthDown ((int) stemIndex); };
		longer.onClick = [this] { motion->lengthUp ((int) stemIndex); };
		lengthLabel.setJustificationType (juce::Justification::centred);
		addAndMakeVisible (lengthLabel);
	}
	setStemColour (stemColour);
	refresh();
	if (motion != nullptr)
		startTimerHz (refreshHz);
}

StemSpatialPopup::~StemSpatialPopup()
{
	stopTimer();
}

void StemSpatialPopup::setStemColour (juce::Colour colour)
{
	stemColour = colour;
	stemViewer.setColour (StemViewer::stemColourId, colour);
	shiftKey.setColour (juce::TextButton::buttonOnColourId, colour);
	refresh();
}

spatial::Pose StemSpatialPopup::homeNow() const
{
	return { controls.azimuth[stemIndex].load(), controls.elevation[stemIndex].load(), controls.width[stemIndex].load() };
}

void StemSpatialPopup::homeDragged (spatial::Pose pose)
{
	controls.azimuth[stemIndex].store (pose.azimuth);
	controls.elevation[stemIndex].store (pose.elevation);
	controls.width[stemIndex].store (pose.width);
	azimuthSlider.setValue (pose.azimuth, juce::dontSendNotification);
	elevationSlider.setValue (pose.elevation, juce::dontSendNotification);
	widthSlider.setValue (pose.width, juce::dontSendNotification);
}

void StemSpatialPopup::pressPad (motionlite::Shape shape)
{
	if (motion == nullptr)
		return;
	const auto seed = (std::uint64_t) juce::Random::getSystemRandom().nextInt64();
	motion->press ((int) stemIndex, shape, shiftKey.getToggleState(), seed);
	shiftKey.setToggleState (false, juce::dontSendNotification);   // SHIFT holds for one press
}

void StemSpatialPopup::refresh()
{
	const auto home = homeNow();
	if (motion == nullptr)
	{
		stemViewer.setShown (home, home, motionlite::PadState::idle);
		return;
	}
	const auto stem = (int) stemIndex;
	stemViewer.setShown (home, motion->posePlayed (stem), motion->stemState (stem));
	lengthLabel.setText (lengthName (motion->lengthBars (stem)), juce::dontSendNotification);
	for (size_t p = 0; p < pads.size(); ++p)
		lookOfPad ((motionlite::Shape) p);
}

// Identity stays (name, place); the state is the face: idle plain, armed
// blinking on the deck's beat, running in the stem's colour with its beat count.
void StemSpatialPopup::lookOfPad (motionlite::Shape shape)
{
	auto& key = pads[(size_t) shape];
	const auto stem = (int) stemIndex;
	const auto state = motion->padState (stem, shape);
	const auto plain = getLookAndFeel().findColour (juce::TextButton::buttonColourId);
	const auto text = getLookAndFeel().findColour (juce::TextButton::textColourOffId);
	juce::String name = padNames[(size_t) shape];

	auto face = plain;
	auto ink = text;
	if (state == motionlite::PadState::armed)
	{
		const auto beat = motion->beat();
		const auto alpha = ! beat ? armedNoBeat : (*beat - std::floor (*beat) < 0.5 ? armedLit : armedDim);
		face = plain.interpolatedWith (stemColour, alpha);
	}
	else if (state == motionlite::PadState::running)
	{
		face = stemColour;
		ink = stemColour.contrasting();
		if (const auto progress = motion->progress (stem))
			name << " " << progress->first << "/" << progress->second;
	}
	key.setColour (juce::TextButton::buttonColourId, face);
	key.setColour (juce::TextButton::textColourOffId, ink);
	key.setButtonText (name);
}

void StemSpatialPopup::resized()
{
	const auto height = (float) getHeight();
	const auto gap = juce::roundToInt (height * gapShare);
	auto area = getLocalBounds().reduced (gap * 2);
	const auto rowHeight = juce::roundToInt (height * sliderRowShare);
	const auto labelWidth = area.getWidth() / 5;

	for (auto [label, slider] : { std::pair { &threeDLabel, &threeDSlider }, std::pair { &azimuthLabel, &azimuthSlider },
								  std::pair { &elevationLabel, &elevationSlider }, std::pair { &widthLabel, &widthSlider } })
	{
		auto row = area.removeFromTop (rowHeight);
		label->setBounds (row.removeFromLeft (labelWidth));
		slider->setBounds (row);
	}

	if (motion != nullptr)
	{
		auto lengthRow = area.removeFromBottom (juce::roundToInt (height * lengthRowShare));
		area.removeFromBottom (gap);
		auto padRow = area.removeFromBottom (juce::roundToInt (height * padRowShare));
		area.removeFromBottom (gap);

		const auto padWidth = padRow.getWidth() / (int) pads.size();
		for (auto& key : pads)
			key.setBounds (padRow.removeFromLeft (padWidth).reduced (gap / 2));
		const auto keyWidth = lengthRow.getWidth() / 5;
		shiftKey.setBounds (lengthRow.removeFromRight (keyWidth * 2).reduced (gap / 2));
		shorter.setBounds (lengthRow.removeFromLeft (keyWidth).reduced (gap / 2));
		longer.setBounds (lengthRow.removeFromRight (keyWidth).reduced (gap / 2));
		lengthLabel.setBounds (lengthRow);
	}
	area.removeFromTop (gap);
	const auto side = juce::jmin (area.getWidth(), area.getHeight());
	stemViewer.setBounds (area.withSizeKeepingCentre (side, side));
}

void StemSpatialPopup::show (juce::Component& anchor, DeckSpatial::Controls& controls, DeckMotion* motion, int stem)
{
	auto* top = anchor.getTopLevelComponent();
	const auto window = top->getLocalBounds();

	auto content = std::make_unique<StemSpatialPopup> (controls, stem, motion);
	content->setStemColour (anchor.findColour (juce::Slider::rotarySliderFillColourId));
	// The viewer wants a square below the sliders: taller than wide, within the window.
	const auto width = window.getWidth() / 3;
	content->setSize (width, juce::jmin (window.getHeight() * 4 / 5, width * 2));

	auto& box = juce::CallOutBox::launchAsynchronously (std::move (content), top->getLocalArea (&anchor, anchor.getLocalBounds()), top);
	box.setLookAndFeel (&anchor.getLookAndFeel());
}
