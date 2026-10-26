// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "StemJob.h"

#include <pthread.h>
#include <sched.h>

// The audio thread gets the CPU the stem separator stays off (StemJob.h), so
// a deck plays on while a track is separated. Called at the top of every
// audio block; pins once per thread: JACK and the ALSA fallback each have their own.
inline void pinAudioThreadToItsCpu()
{
	static thread_local const bool pinned = [] {
		cpu_set_t cpus;
		CPU_ZERO (&cpus);
		CPU_SET (audioCpu, &cpus);
		return pthread_setaffinity_np (pthread_self(), sizeof (cpus), &cpus) == 0;
	}();
	(void) pinned;
}
