# JUCE DJ Application

> A dual-deck DJ application written in C++ with the JUCE framework — real-time audio mixing,
> waveform-based loop editing, crossfading, and a drag-and-drop playlist.

**Language:** C++ · **Framework:** JUCE · **Role:** Solo · **Built:** 2022, BSc Games Programming (Object-Oriented Programming)

---

## What It Does

Two independent decks, each loading its own track, with a crossfader mixing between them.
Each deck offers play/pause/stop, volume, variable playback speed, a scrubable timeline, and a
rendered waveform. Dragging across a deck's waveform sets **loop start and end points**, and
playback loops inside that region until it's cleared. A shared playlist accepts dropped audio
files, persists to disk as CSV, and can load tracks into either deck. Each deck also keeps a
one-track queue that auto-advances when the current song ends.

---

## Technical Implementation

### Real-time audio

`DJAudioPlayer` implements JUCE's `AudioSource` contract — `prepareToPlay`,
`getNextAudioBlock`, `releaseResources` — placing it directly in the audio callback path,
which runs on a high-priority thread under a hard deadline.

Playback is assembled as a source chain: an `AudioFormatReaderSource` reads and decodes the
file, an `AudioTransportSource` handles transport and gain, and a `ResamplingAudioSource`
wraps the transport to provide pitch/speed control by adjusting the resampling ratio. Speed
adjustment is therefore free at mix time — it's a property of the chain, not per-sample work.

Loop handling lives inside the audio callback: each block checks the playhead against the
loop's end point and rewinds to its start when passed, so loops are sample-accurate to block
boundaries rather than dependent on UI timer resolution.

Ownership is explicit — the reader source is held in a `std::unique_ptr` and swapped via
`reset(newSource.release())` on load, so the previous decoder is destroyed deterministically
rather than leaked or double-freed. The format manager is held as a **reference member**,
injected by the owner, since the player uses it but does not own it.

### Crossfading

The crossfader applies a two-stage gain law per deck. Each deck's final gain is
`gain * mixerGain`, where the deck's own volume slider and the shared crossfader position are
kept as independent values and combined only at mix time — so moving the crossfader never
destroys the per-deck volume the user set.

### Waveform display and loop selection

`WaveformDisplay` implements `Component`, `ChangeListener`, and `MouseListener` at once. It
renders from a `juce::AudioThumbnail` backed by a shared `AudioThumbnailCache`, so the two
decks share one cache of decoded peak data rather than each building its own.

It listens for change broadcasts to repaint as the thumbnail finishes loading in the
background, and translates `mouseDown` / `mouseDrag` / `mouseUp` into normalised loop
boundaries handed back to the player as a `LoopObject`. `LoopObject` validates itself —
setting a loop whose end precedes its start marks it invalid rather than producing a
negative-length region the audio thread would have to defend against.

### Component architecture

The GUI classes lean on interface inheritance rather than switch statements. `DeckGUI`
implements six:

```cpp
class DeckGUI : public juce::Component,
                public juce::Button::Listener,
                public juce::Slider::Listener,
                public juce::FileDragAndDropTarget,
                public juce::Timer,
                public Songqueue
```

`Songqueue` is my own abstract base class with a pure virtual `playSong()`, so queue behaviour
is defined once and each deck supplies its own playback action. `PlaylistComponent`
similarly implements `TableListBoxModel` to serve rows on demand instead of holding a parallel
widget tree, plus `FileDragAndDropTarget` for imports, backed by a `std::vector<Song>` and
`std::fstream` CSV persistence.

---

## PIO clock: following the CDJs

With **SYNC: PIO** in the top bar, SYNC on a deck follows the Pioneer Pro DJ Link tempo master
instead of the other deck: its tempo (BPM including pitch) and its beat. Both decks may be synced
at once, each choosing half, same or double tempo for itself. **SYNC: DECK** is the classic
deck-to-deck sync.

- StemDeck joins the Pioneer network as a virtual CDJ, **number 6** (setting `pioDevice`), and
  listens on UDP 50000-50002. The sockets use `SO_REUSEADDR`, so it can run on the same machine
  as beat-analyzer (which is number 7) and both get the broadcast beats.
- The master is read from the CDJs' status packets. Those are partly sent to one address; on a
  machine shared with beat-analyzer they may not arrive here. Without them for two seconds the
  readout says **no master info** and StemDeck follows the player chosen beside it (preset: the
  first one heard).
- If the master falls silent, the tempo is held and the phase is left alone until beats return.
- It aligns the **beat, not the bar**: the track's grid knows beats, not where "1" is. Set the
  downbeat with the jog, as on a CDJ.
- The logic is pure and unit-tested (`Source/ProLinkPackets`, `PioneerClock`, `FollowLeader`;
  build with `-DSTEMDECK_TESTS=ON`, run `ctest`). The network thread is `ProLinkReceiver`, a port
  of beat-analyzer's `PioneerReceiver`.

---

## What I'd Do Differently

Reading this back some years later, the architecture holds up better than the details:

- **`DBG()` logging inside `getNextAudioBlock`.** Logging from the audio callback is a
  real-time audio sin — it can allocate and block on a thread that must never do either. It
  compiles out in release builds, which is exactly why it survived.
- **`getRelativePosition()` and `getPositionRelative()` are byte-for-byte identical.** Two
  names, one behaviour, both public. One should not exist.
- **Unguarded division by track length.** Position maths divides by
  `getLengthInSeconds()` with no zero check, so querying position before a file loads yields
  NaN rather than 0.
- **`loadURL()` auto-starts playback.** Loading a track and playing it are separate concerns;
  fusing them means there is no way to load silently.
- **`#include <Vector>` in `songQueue.h`** — capital V. This compiles on Windows because the
  filesystem is case-insensitive and breaks immediately on Linux or macOS. The same header
  also places `#pragma once` *after* its includes, which defeats the guard's purpose.
- Decks are passed around as raw `DJAudioPlayer*`. Non-owning references would state the
  lifetime relationship the code actually relies on.

---

## Building

`./start.sh` configures (CMake's default generator), builds and starts StemDeck. It looks for
JUCE under `~/local/juce` unless `CMAKE_PREFIX_PATH` is already set. Tests: `cmake -S . -B build -DSTEMDECK_TESTS=ON` and
`ctest --test-dir build`.

This repository contains the application source only. To build it, create a JUCE GUI
Application project (Projucer), add the `Source/` files to it, and enable the
`juce_audio_utils` module. `JuceHeader.h` is generated by Projucer and is intentionally not
committed here.
