#pragma once

// On the rig every finger reaches JUCE twice: as a touch and, a few ms later,
// as X's emulated mouse. A key that toggles on each tap would toggle back.
// Pure: the caller hands in the time.
class TapFilter
{
public:
	// Far below any two taps by hand, far above the twins' 2 ms.
	static constexpr double twinWindowMs = 120.0;

	// True for a tap that counts; false for the twin of the one before.
	bool accept (double nowMs)
	{
		if (nowMs - lastAcceptedMs < twinWindowMs)
			return false;
		lastAcceptedMs = nowMs;
		return true;
	}

private:
	double lastAcceptedMs = -1.0e12;
};

// The library's sort after a tap on a column header: a new column sorts
// forwards, the sorted one reverses; 0 (beside every column) changes nothing.
struct SortOrder
{
	int column = 0;
	bool forwards = true;

	bool operator== (const SortOrder& o) const { return column == o.column && forwards == o.forwards; }
};

inline SortOrder sortAfterTap (SortOrder current, int tappedColumn)
{
	if (tappedColumn == 0)
		return current;
	if (tappedColumn == current.column)
		return { current.column, ! current.forwards };
	return { tappedColumn, true };
}

// A finger on a touch list (the library's table and folder tree).
namespace touchlist
{
	// A press held still this long before it moves drags the row under it (a
	// set onto a deck); a quicker one scrolls the list. JUCE's own long press.
	constexpr double longPressMs = 300.0;
	inline bool isLongPress (double heldMs) { return heldMs >= longPressMs; }

	// A press that ends within a quarter of a row of where it began is a tap;
	// further, it was a scroll.
	inline bool isTap (float distance, int rowHeight) { return distance * 4.0f < (float) rowHeight; }
}
