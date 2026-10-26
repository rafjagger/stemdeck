// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Whether a convolving decoder runs this chunk: while it is wanted, and for a
// hold-over of silence after, so its tail decays instead of being cut and a
// restart finds it empty. Idle, it is not called at all (ruling R12).
class DecoderGate
{
public:
	static constexpr int holdOverSamples = 2048;   // > IEM BinauralDecoder's order-3 IR at 44.1 and 48 kHz

	// Advances by a chunk of numSamples; true: run the decoder for it.
	bool next (bool wanted, int numSamples)
	{
		if (wanted)
		{
			remaining = holdOverSamples;
			return true;
		}
		if (remaining <= 0)
			return false;
		remaining -= numSamples;
		return true;
	}

	void reset() { remaining = 0; }

private:
	int remaining = 0;
};
