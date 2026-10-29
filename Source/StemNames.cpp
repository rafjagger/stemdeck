#include "StemNames.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>

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

std::optional<int> numberedSlot (const std::string& suffix)
{
	if (suffix.size() < 3 || suffix[0] < '1' || suffix[0] > '4')
		return std::nullopt;
	if (std::string (" -_.").find (suffix[1]) == std::string::npos)
		return std::nullopt;
	if (suffix.find_first_not_of (" -_.", 1) == std::string::npos)
		return std::nullopt;
	return suffix[0] - '1';
}

namespace
{
	struct Member
	{
		size_t index;
		std::string suffix;
	};

	StemGrouping loneFile (const std::vector<std::string>& baseNames, size_t index)
	{
		StemGrouping set;
		set.name = baseNames[index];
		set.slots[0] = index;
		set.stemNames[0] = baseNames[index];
		return set;
	}

	// Two or three members, each naming a slot of its own: file n on slot n.
	std::optional<StemGrouping> numberedSet (const std::string& prefix, const std::vector<Member>& members)
	{
		if (members.size() < 2 || members.size() > 3)
			return std::nullopt;

		StemGrouping set;
		set.name = prefix;
		for (const auto& member : members)
		{
			const auto slot = numberedSlot (member.suffix);
			if (! slot || set.slots[(size_t) *slot])
				return std::nullopt;
			set.slots[(size_t) *slot] = member.index;
			set.stemNames[(size_t) *slot] = member.suffix;
		}
		return set;
	}
}

std::vector<StemGrouping> groupStemFiles (const std::vector<std::string>& baseNames,
										  const std::function<bool (const std::string&, const std::string&)>& lessSuffix)
{
	std::vector<StemGrouping> sets;
	std::map<std::string, std::vector<Member>> groups;

	for (size_t i = 0; i < baseNames.size(); ++i)
	{
		std::string prefix, suffix;
		if (splitStemName (baseNames[i], prefix, suffix))
			groups[prefix].push_back ({ i, suffix });
		else
			sets.push_back (loneFile (baseNames, i));
	}

	for (auto& [prefix, members] : groups)
	{
		if (members.size() == 4)
		{
			std::sort (members.begin(), members.end(), [&lessSuffix] (const Member& a, const Member& b)
			{
				return lessSuffix (a.suffix, b.suffix);
			});
			StemGrouping set;
			set.name = prefix;
			for (size_t s = 0; s < members.size(); ++s)
			{
				set.slots[s] = members[s].index;
				set.stemNames[s] = members[s].suffix;
			}
			sets.push_back (set);
		}
		else if (auto numbered = numberedSet (prefix, members))
		{
			sets.push_back (*numbered);
		}
		else
		{
			for (const auto& member : members)
				sets.push_back (loneFile (baseNames, member.index));
		}
	}
	return sets;
}

std::string loadedSlotsText (const std::array<bool, 4>& loaded)
{
	std::string text;
	for (size_t s = 0; s < loaded.size(); ++s)
		if (loaded[s])
			text += (text.empty() ? "" : " ") + std::to_string (s + 1);
	return text;
}
