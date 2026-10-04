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

## Controller, GRID, AUTO DJ, session

The Stanton SCS.3d mapping, GRID, AUTO DJ, the MASTER handover and the session restore are in
the documentation: [StemDeck](https://a3-audio.github.io/a3-doc/user/stemdeck.html)
([SCS.3d](https://a3-audio.github.io/a3-doc/user/stemdeck.html#stemdeck-scs3d),
[GRID](https://a3-audio.github.io/a3-doc/user/stemdeck.html#stemdeck-grid),
[AUTO DJ](https://a3-audio.github.io/a3-doc/user/stemdeck.html#stemdeck-autodj),
[Session](https://a3-audio.github.io/a3-doc/user/stemdeck.html#stemdeck-session)).

## License

No license has been declared yet: the repository has no license file.
