// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MotionLite.h"

#include <algorithm>
#include <cmath>

namespace motionlite
{
	namespace
	{
		constexpr double pi = 3.14159265358979323846;
		constexpr double assumedBpm = 120.0;
		constexpr double minBars = 0.5, maxBars = 8.0;
		constexpr double returnBeats = 1.0;
		constexpr float pingSwing = 60.0f, riseTop = 60.0f, scatMaxElevation = 45.0f;

		float wrapDegrees (double degrees)
		{
			double wrapped = std::fmod (degrees + 180.0, 360.0);
			if (wrapped < 0.0)
				wrapped += 360.0;
			return static_cast<float> (wrapped - 180.0);
		}

		// Counter form of splitmix64: the n-th output of the stream from seed.
		std::uint64_t splitmix (std::uint64_t seed, std::uint64_t n)
		{
			std::uint64_t z = seed + (n + 1) * 0x9E3779B97F4A7C15ull;
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
			return z ^ (z >> 31);
		}

		double unit (std::uint64_t seed, std::uint64_t n)
		{
			return static_cast<double> (splitmix (seed, n) >> 11) * (1.0 / 9007199254740992.0);
		}

		spatial::Pose risePose (const spatial::Pose& home, double t, double length)
		{
			const double fall = std::min (4.0, length / 2.0), riseSpan = length - fall;
			const double r = t < riseSpan ? 0.5 - 0.5 * std::cos (pi * t / riseSpan)
			                              : 0.5 + 0.5 * std::cos (pi * (t - riseSpan) / fall);
			auto pose = home;
			pose.elevation = home.elevation + (riseTop - home.elevation) * static_cast<float> (r);
			return pose;
		}
	}

	spatial::Pose shapePose (Shape shape, const spatial::Pose& home, double t, double lengthBeats, std::uint64_t seed)
	{
		if (t >= lengthBeats || lengthBeats <= 0.0)
			return home;
		t = std::max (t, 0.0);
		const auto beat = static_cast<std::uint64_t> (std::floor (t));
		const bool lastBeat = static_cast<double> (beat) >= lengthBeats - 1.0;
		auto pose = home;

		switch (shape)
		{
			case Shape::orbit:
				pose.azimuth = wrapDegrees (home.azimuth + 360.0 * t / lengthBeats);
				return pose;
			case Shape::ping:
				if (lastBeat)
					return home;
				pose.azimuth = wrapDegrees (home.azimuth + ((beat % 2 == 0) ? -pingSwing : pingSwing));
				return pose;
			case Shape::rise:
				return risePose (home, t, lengthBeats);
			case Shape::scat:
				if (lastBeat)
					return home;
				pose.azimuth = static_cast<float> (-180.0 + 360.0 * unit (seed, 2 * beat));
				pose.elevation = static_cast<float> (scatMaxElevation * unit (seed, 2 * beat + 1));
				return pose;
		}
		return home;
	}

	float envelope (double t, double lengthBeats)
	{
		return static_cast<float> (std::clamp (std::min (t, lengthBeats - t), 0.0, 1.0));
	}

	void StemMotion::setHome (spatial::Pose newHome) { home = newHome; }

	void StemMotion::lengthDown() { lengthBarsValue = std::max (minBars, lengthBarsValue / 2.0); }
	void StemMotion::lengthUp() { lengthBarsValue = std::min (maxBars, lengthBarsValue * 2.0); }

	void StemMotion::press (Shape shape, bool shift, std::uint64_t seed)
	{
		if (running && running->shape == shape)
		{
			if (! running->returning)
				stopRunning();
			return;
		}
		if (! shift && pending && pending->shape == shape)
		{
			pending.reset();
			return;
		}

		Shot shot;
		shot.shape = shape;
		shot.seed = seed;
		shot.lengthBeats = lengthBarsValue * beatsPerBar;
		if (shift)
		{
			running = shot;
			pending.reset();
			return;
		}
		pending = shot;
	}

	PadState StemMotion::state (Shape shape) const
	{
		if (running && running->shape == shape)
			return PadState::running;
		if (pending && pending->shape == shape)
			return PadState::armed;
		return PadState::idle;
	}

	std::optional<std::pair<int, int>> StemMotion::progress() const
	{
		if (! running)
			return std::nullopt;
		const int total = static_cast<int> (running->lengthBeats);
		const int now = running->returning ? total : static_cast<int> (std::floor (running->t)) + 1;
		return std::pair<int, int> { std::clamp (now, 1, total), total };
	}

	void StemMotion::startPending()
	{
		running = pending;
		pending.reset();
	}

	void StemMotion::stopRunning()
	{
		const auto now = frameNow();
		running->returning = true;
		running->returnT = 0.0;
		running->returnFrom = now.pose;
		running->returnEnvelope = now.envelope;
	}

	Frame StemMotion::frameNow() const
	{
		if (! running)
			return { home, 0.0f };

		if (! running->returning)
			return { shapePose (running->shape, home, running->t, running->lengthBeats, running->seed),
			         envelope (running->t, running->lengthBeats) };

		const auto fraction = static_cast<float> (std::clamp (running->returnT / returnBeats, 0.0, 1.0));
		const auto& from = running->returnFrom;
		spatial::Pose pose = home;
		const float turn = std::remainder (home.azimuth - from.azimuth, 360.0f);
		pose.azimuth = wrapDegrees (from.azimuth + turn * fraction);
		pose.elevation = from.elevation + (home.elevation - from.elevation) * fraction;
		return { pose, running->returnEnvelope * (1.0f - fraction) };
	}

	Frame StemMotion::advance (const Clock& clock, double blockSeconds)
	{
		if (running)
		{
			const bool finished = running->returning ? running->returnT >= returnBeats
			                                         : running->t >= running->lengthBeats;
			if (finished)
				running.reset();
		}

		if (pending)
		{
			const bool barCrossed = clock.beat && lastBeat && *clock.beat >= *lastBeat
				&& std::floor (*clock.beat / beatsPerBar) > std::floor (*lastBeat / beatsPerBar);
			if (! clock.beat || barCrossed)
				startPending();
		}
		lastBeat = clock.beat;

		const auto frame = frameNow();

		if (running && clock.playing)
		{
			const double bpm = clock.beat ? clock.bpm : assumedBpm;
			const double beats = std::max (0.0, blockSeconds * bpm * clock.rate / 60.0);
			(running->returning ? running->returnT : running->t) += beats;
		}
		return frame;
	}
}
