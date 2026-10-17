#include "RemotePointer.h"

#include <cmath>

namespace remote
{
	std::optional<PointerEvent> parsePointer (int phase, float x, float y)
	{
		if (phase < 0 || phase > 2 || ! std::isfinite (x) || ! std::isfinite (y))
			return std::nullopt;
		if (x < 0.0f || x > 1.0f || y < 0.0f || y > 1.0f)
			return std::nullopt;
		return PointerEvent { static_cast<PointerEvent::Phase> (phase), x, y };
	}

	std::vector<Inject> RemotePointerGate::release()
	{
		if (! isHeld)
			return {};
		isHeld = false;
		return { { PointerEvent::Phase::up, lastX, lastY } };
	}

	std::vector<Inject> RemotePointerGate::live (bool switchOn, double nowMs)
	{
		if (switchOn)
		{
			on = true;
			lastOnMs = nowMs;
			return {};
		}
		on = false;
		return release();
	}

	bool RemotePointerGate::isLive (double nowMs) const
	{
		return on && nowMs - lastOnMs <= liveLeaseMs;
	}

	std::vector<Inject> RemotePointerGate::pointer (const PointerEvent& e, double nowMs)
	{
		if (! isLive (nowMs))
			return {};

		using Phase = PointerEvent::Phase;
		if (e.phase != Phase::down && ! isHeld)
			return {};

		auto out = e.phase == Phase::down ? release() : std::vector<Inject>();
		lastSeenMs = nowMs;
		lastX = e.x;
		lastY = e.y;
		isHeld = e.phase != Phase::up;
		out.push_back ({ e.phase, e.x, e.y });
		return out;
	}

	std::vector<Inject> RemotePointerGate::tick (double nowMs)
	{
		if (on && ! isLive (nowMs))
			return live (false, nowMs);
		if (isHeld && nowMs - lastSeenMs > releaseAfterMs)
			return release();
		return {};
	}
}
