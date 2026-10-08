#include "GridEdit.h"

#include <cmath>

namespace GridEdit
{
	Grid normalised (Grid grid)
	{
		if (grid.bpm <= 0.0)
			return grid;
		const auto bar = beatsPerBar * 60.0 / grid.bpm;
		grid.firstBeat -= std::floor (grid.firstBeat / bar) * bar;
		return grid;
	}

	Grid shift (Grid grid, double seconds)
	{
		grid.firstBeat += seconds;
		return normalised (grid);
	}

	Grid shiftHalfBeat (Grid grid, bool forwards)
	{
		if (grid.bpm <= 0.0)
			return grid;
		return shift (grid, (forwards ? 0.5 : -0.5) * 60.0 / grid.bpm);
	}

	Grid moveOne (Grid grid, bool later)
	{
		if (grid.bpm <= 0.0)
			return grid;
		return shift (grid, (later ? 1.0 : -1.0) * 60.0 / grid.bpm);
	}

	Grid snapToCue (Grid grid, double cueSeconds)
	{
		grid.firstBeat = cueSeconds;
		return normalised (grid);
	}

	Grid shiftToPhase (Grid grid, double position, double leaderBeatPhase)
	{
		if (grid.bpm <= 0.0)
			return grid;
		const auto beatLength = 60.0 / grid.bpm;
		const auto ownPhase = (position - grid.firstBeat) / beatLength;
		auto error = (ownPhase - std::floor (ownPhase)) - (leaderBeatPhase - std::floor (leaderBeatPhase));
		error -= std::round (error);   // -0.5 .. 0.5: positive, this deck is ahead of its grid's beat
		return shift (grid, error * beatLength);
	}

	Loop snappedLoop (Grid grid, double startSeconds, double endSeconds)
	{
		if (grid.bpm <= 0.0)
			return { startSeconds, endSeconds };

		const auto beatLength = 60.0 / grid.bpm;
		const auto nearest = [&grid, beatLength] (double seconds)
		{
			return grid.firstBeat + std::round ((seconds - grid.firstBeat) / beatLength) * beatLength;
		};

		const auto start = nearest (startSeconds);
		auto end = nearest (endSeconds);
		if (end <= start)
			end = start + beatLength;
		return { start, end };
	}
}
