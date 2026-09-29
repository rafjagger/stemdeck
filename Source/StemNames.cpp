#include "StemNames.h"

#include <array>

namespace
{
	const std::string separators = " -_";

	std::string trimEnd (std::string s, const std::string& characters)
	{
		const auto end = s.find_last_not_of (characters);
		return end == std::string::npos ? std::string() : s.substr (0, end + 1);
	}
}

bool splitStemName (const std::string& baseName, std::string& prefix, std::string& suffix)
{
	const auto splitAt = baseName.find_last_of (separators);
	if (splitAt == std::string::npos || splitAt == 0 || splitAt == baseName.size() - 1)
		return false;

	prefix = trimEnd (baseName.substr (0, splitAt), separators);
	suffix = baseName.substr (splitAt + 1);
	return ! prefix.empty();
}

std::string stemFileName (const std::string& track, int stem, const std::string& extension)
{
	// Demucs' own order, and NI Stems' -- the one DJs know: bus 1 drums.
	static const std::array<const char*, 4> names { "drums", "bass", "other", "vocals" };
	return track + " - " + std::to_string (stem + 1) + "." + names[(size_t) stem] + "." + extension;
}

std::string sanitiseName (const std::string& name, const std::string& fallback)
{
	std::string clean;
	for (const auto c : name)
		clean += (c == '/' || c == '\\' || c == '\0') ? '_' : c;

	// Leading dots would hide the file; trailing separators would be cut off
	// by splitStemName and change the set's prefix.
	const auto start = clean.find_first_not_of (". ");
	clean = start == std::string::npos ? std::string() : clean.substr (start);
	clean = trimEnd (clean, separators);

	return clean.find_first_not_of ("_ ") == std::string::npos ? fallback : clean;
}
