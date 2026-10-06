#include "FolderChoice.h"

FolderChoice::FolderChoice (std::string currentFolder) : current (std::move (currentFolder)), pending (current) {}

void FolderChoice::picked (const std::string& folder)
{
	if (! folder.empty())
		pending = folder;
}

std::optional<std::string> FolderChoice::toApply() const
{
	if (pending == current)
		return std::nullopt;
	return pending;
}
