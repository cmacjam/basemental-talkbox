# Basemental Talk Box

A talk box instrument for **Logic Pro, Pro Tools, Windows and Linux**, plus a standalone app on all three operating systems.

| Platform | Formats | Hosts |
|---|---|---|
| macOS (Apple Silicon + Intel) | AU, VST3, AAX, Standalone | Logic Pro, MainStage, GarageBand, Pro Tools, Ableton, Cubase, Studio One, Reaper, Bitwig |
| Windows 10/11 (x64) | VST3, AAX, Standalone | Pro Tools, Cubase, Studio One, FL Studio, Ableton, Reaper, Bitwig |
| Linux (x86_64) | VST3, LV2, Standalone (ALSA/JACK) | Reaper, Bitwig, Ardour, Carla, Qtractor, Renoise |

Pro Tools itself runs only on macOS and Windows, so Linux gets VST3 and LV2. It's built around the classic funk talk box lead sound: a bright synth lead shaped by the player's mouth, articulated clearly enough to sing whole lyrics.

It has a built-in synth and you play it from MIDI. Your voice on the side chain becomes the "mouth" that shapes the synth, so you hear the synth's pitch speaking your words. No hose in your mouth.

## How the sound is made

| Real rig | In the plug-in |
|---|---|
| Keyboard synth as the sound source | Two built-in carriers. **MINI**: two saws plus a sub square into a 4-pole transistor-ladder filter (analog mono-lead style). **DX**: two-operator FM with modulator feedback (the bright, buzzy 80s digital talker). Mono legato with glide by default, or 8-voice **POLY** for chords. |
| Talk box driver | Band-limited compression-driver model that saturates when you push **Drive**. |
| Plastic hose | A faint, damped pipe resonance (**Tube**). |
| Your mouth shaping the sound | **VOICE** mode measures your vocal tract from the side-chain voice every 2.7 ms with linear prediction (LPC, 20th order at 48 kHz) and runs the synth through that filter. You hear the synth's pitch, not yours. **VOWEL** mode uses a built-in formant mouth (OO / OH / AH / EH / EE) so it talks with no mic. |
| Lips, teeth, breath | **Consonants** mixes your real S, T, K and breath sounds back in. This is what makes words intelligible. |
| Vocal mic | Dynamic-mic voicing (presence peak, low cut). |

Chain: **Synth → Driver → Tube → Mouth → (+ Consonants, Leak) → Mic → Doubler → Room → Volume → safety soft-clip** (never exceeds 0 dBFS, which matters on stage).

It adds no latency and uses about 2% of one CPU core in mono (4% with 6 poly notes).

## Plug in a mic and play (the easy way)

Like a real talk box, the plug-in opens your microphone itself. You don't need any routing.

1. Click the **VOICE IN** button at the top. The **Voice Input** panel lists every mic and interface, marked **BLUETOOTH**, **USB**, **BUILT-IN** or **WIRELESS**.
2. Click your mic. A green **LIVE** light and the level meter confirm it's working.
3. Click **Auto-level** and sing for 3 seconds; the mic gain is set for you.
4. On a multi-input interface, leave the channel on **AUTO** and it follows whichever input you're singing into.

Bluetooth mics (AirPods, headsets) and an iPhone used as a mic work too. Expect about 0.15–0.25 s of lag, and Bluetooth headphones drop to call quality while their mic is in use. Use a wired mic in your interface, or a USB mic, on stage. Unplug and replug whenever you like: it reconnects by itself, and your project remembers the mic.

In the standalone app, the same panel also picks your **speakers or headphones**.

## Or use your DAW's side chain

Switch the panel to **DAW SIDE CHAIN** and route your mic track into the plug-in's mono **Voice (Side Chain)** input:

**Logic Pro / MainStage (AU)**
1. Create a **Software Instrument** track and load **Basemental Talk Box** (AU Instruments → Basemental).
2. Put your mic on an **Audio** track. You can turn that track's volume down; you don't need to hear the dry voice.
3. In the plug-in window, open the **Side Chain** menu (top right) and choose the mic track.

**Pro Tools (AAX)**
1. Create an **Instrument** track and insert **Basemental Talk Box** (Instrument → Basemental).
2. Put your mic on an **Audio** track and add a **Send** to a bus, e.g. "Talk Voice". Set the send to pre-fader if you want the mic fader down.
3. In the plug-in window, set **Key Input** (the side-chain menu, key icon) to that bus.

**Cubase / Nuendo / Studio One / Bitwig / Ableton (VST3)**: load it as an instrument and route the mic track into its side-chain input. Every DAW has a side-chain selector in the plug-in header or the device's routing panel.

**Reaper (VST3 / LV2)**: give the instrument track 4 channels, send the mic track to channels 3/4, and in the plug-in's I/O pins map input 3 to "Voice (Side Chain)".

**Ardour / Carla (LV2 or VST3)**: connect the mic to the plug-in's third audio input, "Voice (Side Chain)".

**Standalone app (Mac, Windows, Linux)**: the input *is* your mic. Open Options → Audio Settings, pick the mic as input (on Linux, ALSA or JACK), and play with a MIDI keyboard or the on-screen keys. Use headphones to avoid feedback.

**Then, in any host:** play the keys and **mouth or whisper the words** into the mic. Watch the **VOICE** and **MOUTH** meters. If the synth doesn't open, turn **Sensitivity** up.

No mic? Switch to **VOWEL** and "talk" with the **mod wheel** (it sweeps the vowel), or automate the **Vowel** knob. Each new phrase opens the mouth from closed (**Wah**, **Talk Time**), which gives you the "wow / yeah" scoop.

### Playing tips
- Play the lead **mono and legato**. Overlapping notes glide instead of retriggering.
- Pitch bend range defaults to ±2 semitones (**Bend**). Scoop into long notes with the wheel.
- In VOICE mode the mod wheel adds vibrato. **Vibrato** also fades in automatically on held notes.
- **Response** low gives crisp, snappy words. Turn it higher for smooth ballad vowels.

## Controls

- **SYNTH**: Cutoff/Bright, Reso/Feedback, Contour, Detune, Sub, Glide, Vibrato, POLY
- **ENVELOPE**: Attack, Decay, Sustain, Release
- **MOUTH** (VOICE mode): Sensitivity, Response, Brightness, Consonants
- **VOWEL** (no mic): Vowel, Wah, Talk Time
- **THE BOX**: Drive, Tube, Leak
- **DOUBLER**, **ROOM**, **MASTER** (Bend, Tune, Volume), plus meters

Factory presets: Dayton Funk Lead, DX Talker, Smooth Ballad Talk, West Coast Lead, Vowel Wah (No Mic), Vowel Robot Chords, Poly Talk Pads, Dirty Driver, Dry Studio.

## Building

### GitHub Actions (recommended)
Push this folder to a GitHub repo. `.github/workflows/build.yml` builds on every push:

| Job | Output |
|---|---|
| **macOS** | Universal AU + VST3 + Standalone (+ AAX), AU checked with `auval`, one `.pkg` installer |
| **Windows** | VST3 + Standalone (+ AAX) `.zip` with install notes |
| **Linux** | VST3 + LV2 + Standalone `.tar.gz` with `install.sh` (user or `--system` install) |
| **dsp-tests** | Offline sound tests at 44.1 / 48 / 96 kHz |

Download the builds from the run's **Artifacts**. If you push a tag like `v1.0.0`, the workflow also attaches them to a GitHub Release.

### Pro Tools (AAX)
1. Sign up free at **developer.avid.com** and download the **AAX SDK**. Avid's licence doesn't allow it in a public repo.
2. Upload the SDK `.zip` somewhere private with a direct download link (for example a release asset in a *private* repo, or a cloud link). Then add that link as a repository secret named **`AAX_SDK_URL`** (Settings → Secrets and variables → Actions).
3. The Mac and Windows jobs then also build `Basemental Talk Box.aaxplugin`. The Mac installer gets an "AAX (Pro Tools)" option.
4. **Testing:** unsigned AAX builds load in **Pro Tools Developer** (free from Avid). **Selling:** retail Pro Tools only loads AAX plug-ins signed with PACE's **wraptool** (free for Avid developers; needs a PACE account and an iLok).

Building locally: `cmake -B build -DAAX_SDK_PATH=/path/to/aax-sdk ...`

### Locally
macOS:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release -j
```
Windows (Visual Studio 2022): `cmake -B build -A x64` then `cmake --build build --config Release`

Linux (Ubuntu/Debian):
```
sudo apt install build-essential cmake libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
  libglu1-mesa-dev mesa-common-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

### Signing for sale
CI signs Mac builds ad hoc. Before selling, sign with your Apple Developer ID and notarize, otherwise Gatekeeper will warn your customers. Signing Windows builds with a code-signing certificate avoids SmartScreen warnings. AAX also needs PACE signing (see above).

## Offline sound test (no JUCE needed)
```
cd tests && g++ -O2 -std=c++17 render_demo.cpp -o render_demo && mkdir -p out && ./render_demo
```
This checks that the lattice filter matches the LPC maths, that the output takes the synth's pitch and not the singer's, that the mouth curve follows the sung vowel, that the gate closes when you stop talking, and that levels are safe at 44.1, 48 and 96 kHz. It also renders demo WAVs to `tests/out/`.

## Code map
- `Source/dsp/CarrierSynth.h`: MINI and DX carrier synths, mono/legato/glide and poly voice handling
- `Source/dsp/Mouth.h`: LPC vocal-tract analysis, interpolated lattice filter, vowel formant mouth, voice gate
- `Source/dsp/TalkBoxFX.h`: driver, tube, mic, doubler, room
- `Source/dsp/TalkBoxEngine.h`: the full chain
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`: the JUCE plug-in (side-chain voice bus) and UI
- `Source/Presets.h`: factory presets

*Basemental Talk Box is an original instrument. It isn't affiliated with or endorsed by any artist or manufacturer. Artist and gear names are used only to describe the style of sound.*
