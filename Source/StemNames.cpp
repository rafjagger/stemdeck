#include "StemNames.h"

#include <array>
#include <cctype>

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
	if (prefix.empty())
		return false;

	// "Title - 1 - drums": the number goes with the name. Only a number of one
	// or two digits in front of a name, both behind a dash -- "Set - 01 - 02"
	// stays two numbers, as before.
	const auto isNumber = [] (const std::string& s)
	{
		return ! s.empty() && s.size() <= 2 && s.find_first_not_of ("0123456789") == std::string::npos;
	};
	const auto dashBetween = [&baseName] (size_t from, size_t to)
	{
		return baseName.substr (from, to - from).find ('-') != std::string::npos;
	};

	if (! isNumber (suffix) && dashBetween (prefix.size(), splitAt + 1))
	{
		const auto numberAt = prefix.find_last_of (separators);
		if (numberAt != std::string::npos && isNumber (prefix.substr (numberAt + 1)))
		{
			const auto title = trimEnd (prefix.substr (0, numberAt), separators);
			if (! title.empty() && dashBetween (title.size(), numberAt + 1))
			{
				suffix = baseName.substr (baseName.find_first_not_of (separators, title.size()));
				prefix = title;
			}
		}
	}
	return true;
}

std::string stemFileName (const std::string& track, int stem, const std::string& extension)
{
	// Demucs' own order, and NI Stems' -- the one DJs know: bus 1 drums.
	static const std::array<const char*, 4> names { "drums", "bass", "other", "vocals" };
	return track + " - " + std::to_string (stem + 1) + " - " + names[(size_t) stem] + "." + extension;
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

std::string stemLaneLabel (const std::string& name, int stem)
{
	const auto number = std::to_string (stem + 1);
	if (name.empty())
		return number;
	if (std::isdigit ((unsigned char) name[0]))
		return name;
	return number + " " + name;
}
