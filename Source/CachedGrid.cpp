#include "CachedGrid.h"

namespace CachedGrid
{
	bool needsNewDownbeat (const Entry& entry)
	{
		return entry.bpm > 0.0 && entry.version < currentVersion && ! entry.corrected;
	}

	Entry withNewDownbeat (Entry entry, double firstBeat)
	{
		if (entry.corrected)
			return entry;
		entry.firstBeat = firstBeat;
		entry.version = currentVersion;
		return entry;
	}
}
