# StemDeck

A stem player for DJs: two decks, each playing a track split into four stems, and a mixer that
sends every stem to any of six buses. It is part of the [A³ Audio](https://github.com/a3-audio/a3-system)
system: buses 1–4 and AUX feed A³ Core, where each stem gets its own channel and can move round
the room. StemDeck is also a Pro DJ Link tempo master when there are no CDJs.

It makes its own stems, too: drop a stereo track on the library and Demucs (`htdemucs`) splits it
into drums, bass, other and vocals while the decks play on. See
[Making stems from a stereo track](https://a3-audio.github.io/a3-doc/user/stemdeck.html#stemdeck-stem-creator).

![StemDeck main window](docs/stemdeck-main.png)

**Documentation: [StemDeck](https://a3-audio.github.io/a3-doc/user/stemdeck.html)** and
[StemDeck × A³ Motion](https://a3-audio.github.io/a3-doc/user/stemdeck-with-motion.html)
(stem sets, decks, mixer, sync, audio routing, the rig service, troubleshooting).
Everything else: https://a3-audio.github.io/a3-doc/

## Build and run

Needs CMake ≥ 3.22, a C++17 compiler, pkg-config, **JUCE 9.0.3** (the A³ pin, `JUCE_VERSION` in a3-system's `installer/roles/base.py`; found via `CMAKE_PREFIX_PATH`,
default `~/local/juce`) with its Linux dependencies, and on Debian:

```sh
apt install libjack-jackd2-dev libflac-dev libvorbis-dev libogg-dev
```

```sh
./start.sh
```

It configures a Release build in `build/` on the first run, rebuilds what changed and starts
StemDeck on JACK, `pw-jack` or ALSA. The binary is `build/StemDeck_artefacts/Release/StemDeck`.
Details: [Build and start](https://a3-audio.github.io/a3-doc/user/stemdeck.html#stemdeck-build).

The stem creator needs a one-time separator setup (about 1 GB):

```sh
sudo apt install ffmpeg python3-venv
tools/setup-separator.sh
```

## Tests

The C++ unit tests need GoogleTest (`libgtest-dev`):

```sh
export CMAKE_PREFIX_PATH=$HOME/local/juce   # the top-level CMakeLists still needs JUCE
cmake -S . -B build -DSTEMDECK_TESTS=ON
cmake --build build --target stemdeck-tests
ctest --test-dir build
```

`ctest` runs what was built; it does not build. The Python tools have their own tests:
`python3 -m unittest discover -s tools/tests`.

## Not in the docs yet

- **Stanton SCS.3d.** Up to two are taken up as they are plugged in, one per deck; the first
  found is deck A (`<VALUE name="scs3dSwap" val="1"/>` in `~/.config/StemDeck/StemDeck.settings`
  swaps them). The protocol follows Mixxx's SCS.3d mapping.

  | SCS.3d | StemDeck |
  |---|---|
  | **FX EQ LOOP TRIG** | mute stem 1–4 |
  | **VINYL** | loop in, pressed again: loop out |
  | **DECK** | loop off, and on again from its start |
  | **top left / top right** of the circle | library: previous / next set |
  | tap the **centre** of the circle | load the selected set (not onto a playing deck) |
  | **GAIN** / **PITCH** slider | channel fader / tempo (relative) |
  | the **ring** of the circle | scratch: touch holds the record, turning scratches |
  | **PLAY CUE SYNC TAP** | play, cue, SYNC, MASTER |

- **GRID** (Grid Adjust, as on a CDJ-3000): the jog moves the whole grid (a turn is 100 ms);
  **‹1/2** / **1/2›** move it half a beat; **SNAP** puts the downbeat on the cue point;
  **SET 1** puts it on the playhead; **SHIFT** takes over a beat you aligned by ear with the jog
  ring; **RESET** brings back the analysed grid. A corrected grid is kept in `analysis.xml`.
- **AUTO DJ** plays at random from the sets the library search shows, each once. It loads the
  next set onto the other deck and mixes on a downbeat 16 bars before the end, with SYNC and an
  equal-power fader cross (10 s unsynced without a beat grid). It never mixes out of a loop.
- **Sync and MASTER.** SYNC: DECK lines up the bars, not only the beats (at the same tempo).
  Stopping the master deck while the other deck plays hands MASTER over to it.
- **Session.** `~/.config/StemDeck/session.xml` is written every two seconds while anything
  changes. Decks, mixer, library and unfinished stem jobs come back on the next start, and a deck
  that was playing plays on.

## License

No license has been declared yet: the repository has no license file.
