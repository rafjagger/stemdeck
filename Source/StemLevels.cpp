#include "StemLevels.h"

std::shared_ptr<const StemHandover::Envelopes> StemLevels::read (const StemSet& set, juce::AudioFormatManager& formatManager,
																 const std::function<bool()>& shouldAbort)
{
	static_assert (StemSet::numStems == StemHandover::numStems);
	auto levels = std::make_shared<StemHandover::Envelopes>();
	constexpr int chunk = 65536;
	juce::AudioBuffer<float> buffer (2, chunk);

	for (int s = 0; s < StemSet::numStems; ++s)
	{
		std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (set.files[(size_t) s]));
		if (reader == nullptr)
			return nullptr;

		const auto channels = juce::jlimit (1, 2, (int) reader->numChannels);
		StemHandover::EnvelopeBuilder builder (reader->sampleRate);
		for (juce::int64 start = 0; start < reader->lengthInSamples; start += chunk)
		{
			if (shouldAbort())
				return nullptr;
			const auto numSamples = (int) juce::jmin ((juce::int64) chunk, reader->lengthInSamples - start);
			reader->read (&buffer, 0, numSamples, start, true, channels > 1);
			builder.add (buffer.getArrayOfReadPointers(), channels, numSamples);
		}
		(*levels)[(size_t) s] = builder.finish();
	}
	return levels;
}
