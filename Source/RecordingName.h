#pragma once

#include <cstdio>
#include <string>

// The file a recording goes to: "StemDeck 2026-09-29 19-05-12.flac" -- sorts
// by time, no characters a file system or a USB stick objects to. Pure.
inline std::string recordingFileName (int year, int month, int day, int hour, int minute, int second)
{
	char name[64];
	std::snprintf (name, sizeof (name), "StemDeck %04d-%02d-%02d %02d-%02d-%02d.flac", year, month, day, hour, minute, second);
	return name;
}
