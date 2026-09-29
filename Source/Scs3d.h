#pragma once

#include <array>
#include <cstdint>
#include <vector>

// The Stanton SCS.3d DaScratch as a deck controller. Pure: no JUCE, testable
// -- what a MIDI message from the device means, and which messages light its
// LEDs. The IDs are the device's own (after Mixxx's SCS.3d mapping):
//
// The six mode buttons: FX LOOP VINYL / EQ TRIG DECK.
//   FX EQ LOOP TRIG      mute stem 1-4             red: muted, blue: playing
//   VINYL                loop in, then out         blue: in marked, purple: looping
//   DECK                 loop off / on again       red: looping, blue: one to go back to
//   top left / right of the circle   library: previous / next set
//   centre of the circle (tap)       load the selected set
//   GAIN slider (left)   channel fader, absolute     LED bar
//   PITCH slider (right) tempo, relative (no jump)   LED bar from the middle
//   circle               scratch pad: touch holds the record, turning
//                        scratches (128 steps a turn, 33 1/3 rpm), release
//                        lets go; the ring shows the platter turning
//   PLAY CUE SYNC TAP    play, cue (CDJ), sync, MASTER
//
// The device is set to MIDI channel 1 and circle mode on start.
namespace scs3d
{
	struct Event
	{
		enum class Type { none, mute, loopInOut, loopToggle, play, cueDown, cueUp, sync, master,
						  gain, pitch, scratchTouch, scratchMove, scratchRelease,
						  previous, next, load };
		Type type = Type::none;
		int stem = -1;       // mute
		double value = 0.0;  // gain 0..1; pitch and scratchMove: steps, + up / clockwise
	};

	Event decode (std::uint8_t status, std::uint8_t data1, std::uint8_t data2);

	// A scratch step in track seconds: 128 a turn, a turn 1.8 s (33 1/3 rpm).
	constexpr double secondsPerScratchStep = 1.8 / 128.0;

	struct Leds
	{
		int deck = 0;                        // lights A or B
		std::array<bool, 4> muted {};
		bool loopInSet = false, looping = false, loopStored = false;
		bool playing = false, atCue = false;
		bool synced = false, syncBent = false, master = false;
		double gain = 0.0;                   // 0..1, the fader's travel
		double pitch = 0.0;                  // -1..1 of the tempo range
		double platter = 0.0;                // 0..1, where the needle is in a turn
	};

	using Message = std::array<std::uint8_t, 3>;

	// Every LED's state as note-on messages; the device sends only changes.
	std::vector<Message> render (const Leds& leds);

	// Sysex: MIDI channel 1, then circle mode (the centre as one ring).
	std::vector<std::vector<std::uint8_t>> initSysex();
	std::vector<std::uint8_t> flatModeSysex();   // on close: back to its plain state
}
