#include "LibraryFolders.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace libraryfolders
{
	namespace
	{
		std::vector<std::string> partsOf (const std::string& folder)
		{
			std::vector<std::string> parts;
			std::string::size_type start = 0;
			while (start <= folder.size())
			{
				const auto end = folder.find ('/', start);
				const auto part = folder.substr (start, end == std::string::npos ? std::string::npos : end - start);
				if (! part.empty())
					parts.push_back (part);
				if (end == std::string::npos)
					break;
				start = end + 1;
			}
			return parts;
		}

		bool nameBefore (const std::string& a, const std::string& b)
		{
			return std::lexicographical_compare (a.begin(), a.end(), b.begin(), b.end(), [] (unsigned char x, unsigned char y)
			{
				return std::tolower (x) < std::tolower (y);
			});
		}

		// Part by part, so a folder's children follow it before its next sibling.
		bool listsBefore (const std::string& a, const std::string& b)
		{
			const auto pa = partsOf (a), pb = partsOf (b);
			for (size_t i = 0; i < std::min (pa.size(), pb.size()); ++i)
			{
				if (nameBefore (pa[i], pb[i])) return true;
				if (nameBefore (pb[i], pa[i])) return false;
				if (pa[i] != pb[i]) return pa[i] < pb[i];
			}
			return pa.size() < pb.size();
		}
	}

	bool isWithin (const std::string& setFolder, const std::string& chosen)
	{
		if (chosen.empty())
			return true;
		if (setFolder.size() < chosen.size() || setFolder.compare (0, chosen.size(), chosen) != 0)
			return false;
		return setFolder.size() == chosen.size() || setFolder[chosen.size()] == '/';
	}

	std::size_t countWithin (const std::vector<std::string>& setFolders, const std::string& chosen)
	{
		return (std::size_t) std::count_if (setFolders.begin(), setFolders.end(),
											[&chosen] (const std::string& f) { return isWithin (f, chosen); });
	}

	std::vector<std::string> folderTree (const std::vector<std::string>& setFolders)
	{
		std::set<std::string> all;
		for (const auto& folder : setFolders)
			for (auto f = folder; ! f.empty(); f = parentOf (f))
				all.insert (f);

		std::vector<std::string> tree (all.begin(), all.end());
		std::sort (tree.begin(), tree.end(), listsBefore);
		return tree;
	}

	std::string validChoice (const std::string& chosen, const std::vector<std::string>& tree)
	{
		return std::find (tree.begin(), tree.end(), chosen) != tree.end() ? chosen : std::string();
	}

	std::string nameOf (const std::string& folder)
	{
		const auto slash = folder.rfind ('/');
		return slash == std::string::npos ? folder : folder.substr (slash + 1);
	}

	std::string parentOf (const std::string& folder)
	{
		const auto slash = folder.rfind ('/');
		return slash == std::string::npos ? std::string() : folder.substr (0, slash);
	}
}
