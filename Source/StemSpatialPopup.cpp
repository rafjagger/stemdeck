// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "StemSpatialPopup.h"

namespace
{
	constexpr int rows = 4;

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
}

StemSpatialPopup::StemSpatialPopup (DeckSpatial::Controls& controls, int stem)
{
	const auto s = (size_t) juce::jlimit (0, DeckSpatial::stems - 1, stem);
	setUp (threeDSlider, threeDLabel, "3D", { 0.0, 1.0 }, 0.01, controls.threeD[s].load(), {}, controls.threeD[s], *this);
	setUp (azimuthSlider, azimuthLabel, "AZ", { -180.0, 180.0 }, 1.0, controls.azimuth[s].load(), " deg", controls.azimuth[s], *this);
	setUp (elevationSlider, elevationLabel, "EL", { -90.0, 90.0 }, 1.0, controls.elevation[s].load(), " deg", controls.elevation[s], *this);
	setUp (widthSlider, widthLabel, "WIDTH", { 0.0, 360.0 }, 1.0, controls.width[s].load(), " deg", controls.width[s], *this);

	// Double-click returns to the value a fresh stem has.
	threeDSlider.setDoubleClickReturnValue (true, 0.0);
	azimuthSlider.setDoubleClickReturnValue (true, 0.0);
	elevationSlider.setDoubleClickReturnValue (true, 0.0);
	widthSlider.setDoubleClickReturnValue (true, spatial::Pose{}.width);
}

void StemSpatialPopup::resized()
{
	auto area = getLocalBounds().reduced (getHeight() / 16);
	const auto rowHeight = area.getHeight() / rows;
	const auto labelWidth = area.getWidth() / 5;

	for (auto [label, slider] : { std::pair { &threeDLabel, &threeDSlider }, std::pair { &azimuthLabel, &azimuthSlider },
								  std::pair { &elevationLabel, &elevationSlider }, std::pair { &widthLabel, &widthSlider } })
	{
		auto row = area.removeFromTop (rowHeight);
		label->setBounds (row.removeFromLeft (labelWidth));
		slider->setBounds (row);
	}
}

void StemSpatialPopup::show (juce::Component& anchor, DeckSpatial::Controls& controls, int stem)
{
	auto* top = anchor.getTopLevelComponent();
	const auto window = top->getLocalBounds();

	auto content = std::make_unique<StemSpatialPopup> (controls, stem);
	content->setSize (window.getWidth() / 3, window.getHeight() / 5);

	auto& box = juce::CallOutBox::launchAsynchronously (std::move (content), top->getLocalArea (&anchor, anchor.getLocalBounds()), top);
	box.setLookAndFeel (&anchor.getLookAndFeel());
}
