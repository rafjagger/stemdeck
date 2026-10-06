#pragma once

#include <optional>
#include <string>

// The library folder picked in Settings, held until OK. Cancel just drops it.
class FolderChoice
{
public:
	explicit FolderChoice (std::string currentFolder);

	// From the file chooser; an empty path means it closed without a choice.
	void picked (const std::string& folder);

	const std::string& shown() const { return pending; }
	// The folder OK applies, or nothing when it is the one in use already.
	std::optional<std::string> toApply() const;

private:
	std::string current;
	std::string pending;
};
