#pragma once

#include <optional>
#include <vector>

// A pointer from the pro-DJ app, relayed by Core, accepted only while pro-DJ
// LIVE is on. This part is the gate: pure, no JUCE, and the same decision
// Motion makes (namespace a3 there). The lease and the release timers exist
// so that a lost "off" or a lost "up" datagram never leaves a finger held.
namespace remote
{
	struct PointerEvent
	{
		enum class Phase { down, move, up };
		Phase phase = Phase::down;
		float x = 0.0f, y = 0.0f;
	};

	// phase 0..2, x and y finite and within 0..1; anything else is refused.
	std::optional<PointerEvent> parsePointer (int phase, float x, float y);

	struct Inject
	{
		PointerEvent::Phase phase = PointerEvent::Phase::down;
		float x = 0.0f, y = 0.0f;
	};

	// The remote finger is its own touch source: JUCE on X delivers every real
	// touch twice (touch#0 and the emulated mouse#0), so index 0 is taken.
	constexpr int remoteTouchIndex = 9;

	class RemotePointerGate
	{
	public:
		static constexpr double releaseAfterMs = 1000.0, liveLeaseMs = 12000.0;

		// Off while held: an up at the last position.
		std::vector<Inject> live (bool on, double nowMs);
		// Nothing while off; move/up without a down: nothing; down while
		// held: the old one is released first.
		std::vector<Inject> pointer (const PointerEvent& e, double nowMs);
		// The lease run out switches off (with an up if held); a held pointer
		// silent for more than a second is released.
		std::vector<Inject> tick (double nowMs);
		bool isLive (double nowMs) const;
		bool held() const { return isHeld; }

	private:
		std::vector<Inject> release();

		bool on = false, isHeld = false;
		double lastOnMs = 0.0, lastSeenMs = 0.0;
		float lastX = 0.0f, lastY = 0.0f;
	};
}
