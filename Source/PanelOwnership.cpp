// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PanelOwnership.h"

#include <cctype>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <sys/file.h>
#include <system_error>
#include <unistd.h>

namespace panel
{
	namespace
	{
		struct Lock
		{
			char letter = 0;
			int pid = 0;
		};

		// Nothing for no file, an empty or garbled one.
		std::optional<Lock> read (const std::filesystem::path& file)
		{
			std::ifstream in (file);
			Lock lock;
			if (in >> lock.letter >> lock.pid && std::isalpha ((unsigned char) lock.letter) && lock.pid > 0)
				return lock;
			return std::nullopt;
		}

		bool alive (int pid)
		{
			return ::kill (pid, 0) == 0 || errno == EPERM;
		}

		// The kernel's name for a process; empty when it cannot be read.
		std::string programOf (int pid)
		{
			std::string name;
			std::ifstream in ("/proc/" + std::to_string (pid) + "/comm");
			std::getline (in, name);
			return name;
		}

		// Alive and running this same program, not merely a pid someone else got later.
		bool heldByStemDeck (int pid)
		{
			return alive (pid) && programOf (pid) == programOf ((int) ::getpid());
		}

		bool freeLock (const std::optional<Lock>& lock)
		{
			return ! lock || ! heldByStemDeck (lock->pid);
		}

		// Written whole beside the lock and renamed over it: a reader sees the old or the new, never half.
		void write (const std::filesystem::path& file, char letter, int pid)
		{
			std::error_code ignored;
			std::filesystem::create_directories (file.parent_path(), ignored);
			auto temporary = file;
			temporary += ".tmp." + std::to_string (pid);
			{
				std::ofstream out (temporary, std::ios::trunc);
				out << letter << ' ' << pid << '\n';
			}
			std::filesystem::rename (temporary, file, ignored);
		}

		// Exclusive for as long as it lives; changes to the lock are made under it.
		class Guard
		{
		public:
			explicit Guard (const std::filesystem::path& file)
			{
				std::error_code ignored;
				std::filesystem::create_directories (file.parent_path(), ignored);
				auto path = file;
				path += ".guard";
				descriptor = ::open (path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
				if (descriptor >= 0)
					::flock (descriptor, LOCK_EX);
			}
			~Guard()
			{
				if (descriptor >= 0)
					::close (descriptor);   // the flock goes with it
			}
			Guard (const Guard&) = delete;
			Guard& operator= (const Guard&) = delete;

		private:
			int descriptor = -1;
		};
	}

	std::filesystem::path lockFilePath (const char* runtimeDir, unsigned uid, const std::filesystem::path& fallbackBase)
	{
		if (runtimeDir != nullptr && *runtimeDir != 0)
			return std::filesystem::path (runtimeDir) / "stemdeck-panel.lock";
		const auto folder = fallbackBase / ("stemdeck-" + std::to_string (uid));
		std::error_code ignored;
		std::filesystem::create_directories (folder, ignored);
		std::filesystem::permissions (folder, std::filesystem::perms::owner_all, ignored);
		return folder / "panel.lock";
	}

	Ownership::Ownership (std::filesystem::path lockFile, char deckLetter, int processId)
		: file (std::move (lockFile)), letter (deckLetter), pid (processId) {}

	int Ownership::currentProcess() { return (int) ::getpid(); }

	bool Ownership::holds() const
	{
		const auto lock = read (file);
		return lock && lock->letter == letter && lock->pid == pid;
	}

	bool Ownership::claimByDefault()
	{
		return letter == 'A' && claimIfFree();
	}

	bool Ownership::claimIfFree()
	{
		const Guard guard (file);
		if (holds())
			return true;
		if (! freeLock (read (file)))
			return false;
		write (file, letter, pid);
		return holds();
	}

	void Ownership::take()
	{
		const Guard guard (file);
		write (file, letter, pid);
	}

	void Ownership::release()
	{
		// Under the guard nobody can take it between this check and the remove.
		const Guard guard (file);
		if (! holds())
			return;
		std::error_code ignored;
		std::filesystem::remove (file, ignored);
	}

	bool claimAtStart (Ownership& ownership, std::optional<bool> storedPreference)
	{
		if (! storedPreference)
			return ownership.claimByDefault();
		return *storedPreference && ownership.claimIfFree();
	}

	bool applyFollow (Ownership& ownership, PanelFollow::Action action)
	{
		using Action = PanelFollow::Action;
		if (action == Action::release)
			ownership.release();
		else if (action == Action::takeAfterDelay)
			ownership.take();
		return ownership.holds();
	}

	void PanelGain::update (bool holds)
	{
		if (holds && ! wasHolding)
			onGained();
		wasHolding = holds;
	}

	ToggleFace toggleFace (bool followsWorkspace, bool holds)
	{
		if (followsWorkspace)
			return { "Panel follows workspace", false, holds };
		return { "Panel: this deck", true, holds };
	}
}
