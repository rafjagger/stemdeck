# StemDeck

A stem player for DJs: two decks, each playing a track split into four stereo stems, and a
mixer that sends every stem to any of six output buses.

StemDeck is part of the [A³ Audio](https://github.com/a3-audio) system, a live 3D/ambisonics
setup. Its buses 1–4 and AUX feed A³ Core, where a stem sent to aux can be put onto a movement
and flown around the room; a sixth bus, PHONES, is for the headphones.

<!-- IMAGE: the whole main window with a set loaded on both decks, one of them playing -->
![StemDeck main window](docs/stemdeck-main.png)

---

## Stem sets

A set is exactly four audio files in one folder that share a name prefix and differ only in the
part after the last space, `-` or `_`:

```
Artist - Title-001.wav  …  Artist - Title-004.wav
Artist - Title - 01.wav …  Artist - Title - 04.wav
Artist - Title - DUB.wav, … - KICK.wav, … - PADS.wav, … - PERC.wav
```

The suffixes are sorted naturally (`01` before `10`, and words alphabetically), and stem N goes
to bus N. A group with three or five files is not a set and does not show up. Any format JUCE
reads out of the box works (WAV, AIFF, FLAC, Ogg Vorbis); WAV and AIFF are memory-mapped, which
makes seeking and looping instant.

### The library: artist > album > sets

Put the stems in folders by artist and album:

```
stems/
├── Burial/
│   └── Untrue/
│       ├── Archangel - 01.wav … Archangel - 04.wav
│       └── Etched Headplate - Drums.flac, - Bass.flac, - Keys.flac, - Vox.flac
└── Aphex Twin/
    └── Drukqs/
        └── CD1/
            └── Avril 14th - 1.wav … Avril 14th - 4.wav
```

The first folder under the library folder is the **artist**, the second the **album**; deeper
folders are added to the album (`Drukqs / CD1`). A set lying straight in the library folder has
neither, one level down only an artist. Within a folder, four files with the same beginning are a
set, their endings sorted naturally — `1 2 3 10`, `01 … 04`, or alphabetically.

The library table shows **Artist | Album | Set | BPM | Stems | Length**, sorted artist → album →
set to begin with; any header sorts by that column (artist and album keep their sets together and
in order). The search box finds sets by name, artist or album.

The library scans its folder recursively. It starts in `./stems` (relative to where StemDeck was
started) and remembers the last folder you picked.

---

## The screen

Scrolling waveforms of both decks across the top, deck A | mixer | deck B in the middle, the
library at the bottom. The top bar shows the audio status (JACK client, rate, buffer, connected
ports, xruns) and the SYNC source. Some on-screen labels are still German.

### Decks

<!-- IMAGE: one deck close-up: title, overview waveform with a loop set, CUE/PLAY, jog wheel, BPM readout with "Original", SYNC, range button, tempo fader -->
![A deck](docs/stemdeck-deck.png)

Laid out like a CDJ:

- **CUE** works like a CDJ (Mixxx's CDJ mode): while playing, it jumps back to the cue point and
  stops; while stopped, it sets the cue point, or, if already on it, plays for as long as it is
  held.
- **PLAY** starts and pauses.
- **Overview waveform** (four stem lanes): click to jump, drag to set a loop. **LOOP AUS** clears
  it; **REPEAT** starts the track over at its end.
- **Jog wheel**: in **VINYL** mode the platter scratches, forwards or backwards; the outer ring
  bends the pitch while playing and searches while stopped. The mouse wheel nudges or fine-searches.
- **Tempo fader** with a range button cycling ±8 / ±16 / ±50 %.
- **BPM**: the current tempo, with the track's own tempo ("Original") below. It comes from a
  tempo analysis that runs in the background when a set is loaded; it assumes a constant tempo,
  sums all four stems, and caches its result, so each set is analysed once.
- **SYNC**: see [Sync](#sync).
- **GRID**: Grid Adjust, as on a CDJ-3000, for a beat grid the analysis got wrong. While it is
  on, turning the jog wheel moves the whole grid (a turn is 100 ms, a mouse wheel notch about
  2 ms), and a row of buttons appears under the wheel:
  - **‹1/2** / **1/2›** move the grid half a beat — for an analysis that took the off-beats;
  - **SNAP** puts the downbeat (the 1 of the bar) on the cue point: set CUE on the real first
    beat of a bar, press SNAP;
  - **SHIFT** takes over what you aligned by ear, as SHIFT GRID on a CDJ-3000: with SYNC on,
    bend the deck with the jog ring — SYNC then follows only the tempo (the button reads
    **SYNC BPM**) and leaves the beat to you — bring the beats together by ear against the
    leader (the other deck, or the Pioneer master under SYNC: PIO), then press SHIFT. The grid
    moves to where you put the beat, and SYNC follows the beat again. It works with SYNC off
    too;
  - **RESET** brings back the grid as analysed.

  A corrected grid is kept in `analysis.xml` beside the analysed one and used from then on.

The scrolling waveforms scroll past a fixed playhead; drag to move through the track, mouse wheel
to zoom.

### Stanton SCS.3d controllers

Up to two **Stanton SCS.3d DaScratch** are taken up as they are plugged in (looked for every
three seconds), one per deck; the A/B light on each says which. The first found is deck A — set
`<VALUE name="scs3dSwap" val="1"/>` in `~/.config/StemDeck/StemDeck.settings` to swap them.

| SCS.3d | StemDeck |
|---|---|
| **FX LOOP VINYL EQ** | mute stem 1–4 (red: muted, blue: playing) |
| **TRIG** | loop in: marks the point (blue) |
| **DECK** | loop out: loops from the mark to here (red); while looping, loop off |
| **GAIN** slider | channel fader; the LED bar shows it |
| **PITCH** slider | tempo, relative (no jump when you touch it); the LEDs show it from the middle |
| **circle** | scratch pad: touch holds the record, turning scratches (128 steps a turn at 33⅓), letting go lets it run on; one light goes round with the platter |
| **PLAY CUE SYNC TAP** | play, cue (CDJ), SYNC (purple: bent, tempo only), MASTER |

The controller moves the on-screen controls, so everything stays in step either way. The
protocol (sysex setup, IDs, LEDs) follows Mixxx's SCS.3d mapping.

### Auto-DJ

**AUTO DJ** in the top bar plays on its own, track after track, mixed like a DJ would:

- It picks at random from the sets the library shows — so the search box is its playlist: type
  an artist, an album or a word and it plays from those. Each set once, until all were played.
- With nothing playing it starts a track on deck A; a deck already playing it takes as it is.
- Half a minute before the mix it loads the next track onto the other deck, fader down.
- The mix starts on a downbeat, 16 bars before the end of the playing track: the new track
  from its first downbeat, with SYNC on (tempo, beats and bars), and over those 16 bars the
  channel faders cross (equal power). The button shows how far (`MIX 40 %`).
- Then the old deck stops and SYNC comes off; the new track keeps the tempo the mix gave it.
- A track without a beat grid is crossfaded over 10 s, unsynced.

You can play along — change stems, routing, knobs; the Auto-DJ only moves the faders during a
mix. Turning it off leaves everything as it is. It stays on across a restart.

### Mixer

<!-- IMAGE: the mixer between the decks: both channel strips with stem knobs, M and the 2×3 bus switches 1 2 3 / 4 A P per stem, channel faders, PHONES buttons, and the output meters 1-4 / AUX / PH -->
![The mixer](docs/stemdeck-mixer.png)

One channel strip per deck. Per stem: a gain knob (−60 to +6 dB, double-click for 0 dB), **M**
(mute) and six bus switches, right-aligned in two rows: **1 2 3** / **4 A P** — A is AUX, P is
PHONES (lit, they show in AUX and PHONES colour). A stem plays on every bus that is lit — any
number at once, none for silence; a new set starts with stem N on bus N. Buses 1–4 and AUX are
**post fader**, **PH** (PHONES) is **pre fader**. Knob and mute act on all of them. Below the
stems, the channel fader and **PHONES**, which puts the whole deck on the phones bus, pre fader.
In the middle, output meters for buses 1–4, AUX and PH.

### Library

<!-- IMAGE: the library with a few sets listed, BPM column filled, the search box and the "Laden in A / Laden in B" buttons -->
![The library](docs/stemdeck-library.png)

Columns: set, BPM (once analysed), stem names, length, folder; click a header to sort. The search
box filters as you type. Load a set with the **Laden in A / B** buttons, by double-click or Return
(first deck that is not playing), or by dragging a row onto a deck or its waveform.

### Creating stems from stereo files

Drop stereo files, or a whole folder, on the library, or press **Stems erstellen…**. FLAC, WAV,
MP3, AIFF, OGG, M4A and Opus work. StemDeck asks once for the whole batch: the **target
folder**, relative to the library — typed (`Artist/Album`) or picked with **Durchsuchen…**, and
empty for the library folder itself. It is preset from where the files lie: `Artist/Album` for
`…/Artist/Album/*.flac`, only `Artist` when they come from several of its albums. Every track
of the batch lands in that one folder and becomes a set:

```
stems/Artist/Album/Title - 1 - drums.flac
                   Title - 2 - bass.flac
                   Title - 3 - other.flac
                   Title - 4 - vocals.flac
                   originals/Title.flac     (a copy; the library ignores this folder)
```

The title is the file name without its extension (`Artist - Title.flac` gives
`Artist - Title - 1 - drums.flac`). The stems keep the original's format: FLAC, WAV and AIFF at
24 bit, Ogg Vorbis at quality 8. MP3, M4A and Opus become FLAC — StemDeck can't play those, and
FLAC loses nothing a second time.

The separation is [Demucs](https://github.com/adefossez/demucs) `htdemucs` at
44.1 kHz. It runs in the background, one track at a time, **while the decks play on**:
StemDeck's audio thread is pinned to **CPU 1**, and the separator takes every other core
(`0,2-5` on six) at idle priority for CPU and disk, with at most 6 GB of memory. To leave it
fewer cores, set `<VALUE name="separatorCores" val="1"/>` in
`~/.config/StemDeck/StemDeck.settings` (1 is CPU 0 only; a track then takes about 1.2× its
length). The strip under the library bar shows the track,
the progress and how many wait; **Abbrechen** stops the running one and leaves nothing behind.
When a set is done the library rescans and selects it. A track name already in the album gets
` (2)`.

Setup, once (about 1 GB):

```sh
sudo apt install ffmpeg python3-venv
tools/setup-separator.sh
```

The script installs Demucs and the CPU build of PyTorch into
`~/.local/share/StemDeck/separator` and downloads the model. If it is missing, the strip says
so when a job starts.

### Keyboard

| Key | Deck A | Deck B |
|---|---|---|
| Play / pause | `D` | `L` |
| Cue (hold to preview) | `S` | `K` |
| Mute stem 1–4 | `1` `2` `3` `4` | `7` `8` `9` `0` |

---

## Audio output

StemDeck is a JACK client named `StemDeck` with 12 output ports and 2 inputs:

```
deck1_L deck1_R … deck4_L deck4_R   bus 1–4: every stem switched to it, post fader
aux_L   aux_R                        bus AUX: every stem switched to it, post fader
phones_L phones_R                    bus PH: stems switched to it and decks on PHONES, pre fader
```

The names `deck1` … `deck4` are the buses', kept from when bus N was always stem N.

Two **input** ports record: `rec_L` and `rec_R`. **REC** in the top bar writes them to a 24-bit
FLAC in `~/Music/StemDeck-Recordings/`, named by the time (`StemDeck 2026-09-29 19-05-12.flac`);
the button shows the running time, the two small meters beside it the inputs' level — also
while not recording, to see that something is connected. The file is written by a background
thread from a ten-second buffer; should the disk ever fall that far behind, the button shows
`!` and the tooltip says so. Recording needs JACK.

- The ports are **never connected automatically**. Route them with qjackctl, a patchbay or
  whatever you like.
- StemDeck never starts a JACK server. `./start.sh` uses a running `jackd`/`jackdbus`; if there is
  none but PipeWire is running, it starts StemDeck through `pw-jack`. Without either, StemDeck
  falls back to a regular audio device (ALSA), chosen with the **Audio-Einstellungen** button; with
  fewer than 12 outputs the buses are summed down onto the ones there are.
- It takes the graph's sample rate and buffer size as they are and resamples the stems itself. It
  requests nothing on purpose — changing a running graph ends clients like zita-j2n. Set the rate
  beforehand if it matters, e.g. for PipeWire (until restart):

  ```sh
  pw-metadata -n settings 0 clock.force-rate 44100
  ```

---

## Sync

<!-- IMAGE: the top bar, right side: the PIO status readout (e.g. "PIO 128.0 · CDJ 2"), the CDJ player box if visible, the "SYNC: PIO" button and "Audio-Einstellungen" -->
![Top bar with the SYNC source](docs/stemdeck-topbar.png)

The **SYNC: DECK | PIO** button in the top bar chooses what SYNC on a deck follows. Switching it
turns off every SYNC that was on.

Both sources follow with the same rules: the tempo is the leader's times half, same or double —
whichever needs the smallest change, chosen once when SYNC goes on — and the tempo range widens
if it has to. The phase is only corrected while both play and nobody scratches: more than 50 ms
off jumps into phase, closer is nudged by at most 2 %.

### DECK

One deck follows the other. SYNC on deck A makes A follow B; pressing SYNC on B hands the role
over. Like a CDJ's BEAT SYNC it lines up the **bars** too, not only the beats: the follower's
downbeat lands on the leader's (at the same tempo; at half or double tempo only the beats). A
downbeat that is wrong in the grid shows up here first — correct it with [GRID](#deck).

### PIO: following the CDJs

SYNC follows the Pioneer Pro DJ Link tempo master: its tempo (BPM including pitch) and its beat.
Both decks may be synced at once, each with its own half/same/double choice.

- StemDeck joins the network as virtual CDJ **number 6** (setting `pioDevice` in
  `~/.config/StemDeck/StemDeck.settings`) and listens on UDP 50000–50002. The sockets use
  `SO_REUSEADDR`, so it can share a machine with beat-analyzer (number 7) and both get the
  broadcast beats. If the network is not up yet, it retries every two seconds.
- The master comes from the CDJs' status packets. Some of those go to one address only, so on a
  machine shared with beat-analyzer they may not arrive. After two seconds without them the
  readout says **no master** and StemDeck follows the player chosen in the box beside it (by
  default the first one it heard).
- If the master falls silent, the tempo is held (readout: **held**) and the phase left alone
  until beats return.
- It aligns the **beat, not the bar**: the track's grid knows beats, not where the "1" is. Set
  the downbeat with the jog, as on a CDJ.

---

### MASTER: StemDeck as the tempo master

No CDJs on the link? Then StemDeck is the CDJ. Each deck has a **MASTER** button, as on a CDJ:
the master deck's beat goes out to the Pioneer network, and anything following the Pro DJ Link
tempo master follows StemDeck — in the A³ system that is beat-analyzer in clock mode 2, which
passes the beat on to A³ Motion.

- Press MASTER on a deck to make it master; press it on the other deck to hand over; press it on
  the master to turn MASTER off (StemDeck then leaves the Pioneer network if it is not
  following either). With no master chosen, the only playing deck becomes master by itself —
  but not under **SYNC: PIO** (a real CDJ may hold master, and two masters would flip every
  listener's clock) and not once MASTER was turned off by hand. **Stopping the master while the
  other deck plays hands MASTER over to it**, as pausing the sync master does on a CDJ-3000.
- StemDeck sends, as virtual CDJ 6: a **beat packet on every beat** of the master deck (tempo =
  the track's BPM × the tempo fader, beat in the bar counted from the grid's first beat) and a
  **status packet every 200 ms** (master, playing or not, tempo, beat). A stopped master with
  no other deck playing stays master and simply sends no beats.
- Everything goes out as **broadcast**, so a listener on the same machine hears it whichever
  program started first (unicast would reach only one of them).
- A separate thread times the beats to about a millisecond, from the deck's position carried to
  "now"; the 60 Hz screen timer would be up to 16 ms off. A beat is never lost to a late wake-up,
  a cue on a beat sends that beat when you press PLAY, a jump sends no burst of skipped beats, and
  a handover never doubles a beat.
- Nothing is sent while the master deck has no tempo grid yet (still being analysed).
- The readout in the top bar says `PIO master: A` (or B) while StemDeck sends.
- The packets follow the layout [prolink-connect](https://github.com/EvanPurkhiser/prolink-connect)
  reads, so tools built on it (e.g. prolink-tools) see StemDeck like a CDJ.

## Build and run

Linux. You need:

- **CMake** ≥ 3.22, a C++17 compiler, **pkg-config**
- **JUCE 9**, installed so that `find_package(JUCE)` finds it, plus JUCE's own Linux build
  dependencies (X11, freetype, …; see JUCE's `docs/Linux Dependencies.md`). `start.sh` looks
  under `~/local/juce` unless `CMAKE_PREFIX_PATH` is set. To install it there from a JUCE
  checkout:

  ```sh
  cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/local/juce
  cmake --build build --target install
  ```

- via pkg-config: `jack`, `flac`, `vorbisenc`, `vorbisfile`, `vorbis`, `ogg`. On Debian:

  ```sh
  apt install libjack-jackd2-dev libflac-dev libvorbis-dev libogg-dev
  ```

- **GoogleTest** (`libgtest-dev`), only for the tests.

Then:

```sh
./start.sh
```

It configures a Release build in `build/` on the first run (CMake's default generator, so no
Ninja needed), rebuilds incrementally when something changed, and starts StemDeck with the JACK /
`pw-jack` / ALSA choice described above. The binary is
`build/StemDeck_artefacts/Release/StemDeck`.

Settings (library folder, sync source, audio device, window) live in `~/.config/StemDeck/`, next to
`analysis.xml`, the tempo analysis cache, and `session.xml`: what StemDeck picks up again on the
next start. Both decks come back with their set, position, cue, loop, tempo, SYNC and MASTER,
the mixer with every knob, mute, bus switch, fader and PHONES, the library with its sort,
search and selection, and the stem creator with the tracks it had not finished (the running
one starts again). **A deck that was playing plays on** — after a crash or a power cut too,
since the session is written every two seconds while anything changes.

### Tests

The Pro DJ Link parser, the Pioneer clock and the follow rules are pure code with unit tests:

```sh
export CMAKE_PREFIX_PATH=$HOME/local/juce   # the top-level CMakeLists still needs JUCE
cmake -S . -B build -DSTEMDECK_TESTS=ON
cmake --build build --target stemdeck-tests   # or without --target: app and tests
ctest --test-dir build
```

`ctest` runs what was built; it does not build. Tests live in `tests/`.
