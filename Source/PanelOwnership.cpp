// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PanelOwnership.h"

#include <cctype>
#include <fcntl.h>
#include <fstream>
#include <system_error>
#include <unistd.h>

namespace panel
{
	namespace
	{
		// The letter in the file; 0 for no file, an empty or unreadable one.
		char letterIn (const std::filesystem::path& file)
		{
			std::ifstream in (file);
			char letter = 0;
			if (in >> letter && std::isalpha ((unsigned char) letter))
				return letter;
			return 0;
		}

		void write (const std::filesystem::path& file, char letter)
		{
			std::error_code ignored;
			std::filesystem::create_directories (file.parent_path(), ignored);
			std::ofstream (file, std::ios::trunc) << letter << '\n';
		}
	}

	Ownership::Ownership (std::filesystem::path lockFile, char deckLetter) : file (std::move (lockFile)), letter (deckLetter) {}

	bool Ownership::holds() const
	{
		return letterIn (file) == letter;
	}

	bool Ownership::claimByDefault()
	{
		return letter == 'A' && claimIfFree();
	}

	bool Ownership::claimIfFree()
	{
		if (holds())
			return true;
		if (letterIn (file) != 0)
			return false;

		// Created exclusively: two instances starting in the same instant do not both win.
		std::error_code ignored;
		std::filesystem::create_directories (file.parent_path(), ignored);
		std::filesystem::remove (file, ignored);   // an empty or garbled leftover
		const auto fd = ::open (file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
		if (fd < 0)
			return holds();
		const char text[] = { letter, '\n' };
		const auto written = ::write (fd, text, sizeof text);
		::close (fd);
		return written == (ssize_t) sizeof text && holds();
	}

	void Ownership::take()
	{
		write (file, letter);
	}

	void Ownership::release()
	{
		if (! holds())
			return;
		std::error_code ignored;
		std::filesystem::remove (file, ignored);
	}
}
