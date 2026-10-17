# Vocal Editor

**A Product by Avlirecordlabel**

Professional Vocal Editing • Intelligent Analysis • Precision Control

Standalone (Windows) + **VST3** (Cubase and other hosts)

---

## Current status

| Phase | Focus | Status |
|-------|--------|--------|
| **1** | JUCE structure, Waveform, Timeline, selection, transport | Done |
| **2** | Events, Gain, Fade, Envelope, Split, multi-select, project, export | Done (core) |
| **3** | Silence detect, Auto Segment, Level Phrases | Core done |

---

## Requirements (Windows build)

- **Windows 10/11 x64**
- **Visual Studio 2022** (Desktop development with C++)
- **CMake ≥ 3.22**
- **Git**
- **JUCE 8.0.4** (cloned next to the repo)

Optional: Cubase 12/13/14 for VST3 testing

---

## Build steps (Windows)

Open **x64 Native Tools Command Prompt for VS 2022** or a Developer PowerShell.

```bat
:: 1) Clone project
git clone https://github.com/alivalipourmusic/VocalEditor.git
cd VocalEditor

:: 2) Clone JUCE as sibling of VocalEditor
cd ..
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
cd VocalEditor

:: 3) Configure
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64

:: 4) Build Release
cmake --build . --config Release
```

### Expected outputs

| Target | Typical path under `build/` |
|--------|-----------------------------|
| **Standalone** | `VocalEditor_artefacts\\Release\\Standalone\\Vocal Editor.exe` |
| **VST3** | `VocalEditor_artefacts\\Release\\VST3\\Vocal Editor.vst3` |

Search for `Vocal Editor.exe` and `Vocal Editor.vst3` inside `build` if paths differ.

### Install VST3 for Cubase

Copy the whole `Vocal Editor.vst3` folder to:

- `C:\\Program Files\\Common Files\\VST3\\`

Then rescan plug-ins in Cubase.

---

## Test checklist

### A. Build
- [ ] CMake configure succeeds
- [ ] Release build succeeds (Standalone + VST3)
- [ ] Standalone starts without crash

### B. Phase 1-2 core
- [ ] Open WAV
- [ ] Waveform + timeline visible
- [ ] Zoom / Fit
- [ ] Play / Stop
- [ ] Select region -> Create Event
- [ ] Gain change + Undo
- [ ] Fade In/Out from panel
- [ ] Drag fade handles
- [ ] Envelope points (Alt+click / drag)
- [ ] Double-click Split
- [ ] Multi-select + Delete
- [ ] Save .veproj -> reopen -> events restored
- [ ] Segment sliders restored from project
- [ ] Export WAV and audition

### C. Phase 3
- [ ] Auto Segment creates phrases
- [ ] Change Threshold and re-run
- [ ] Word preset
- [ ] Level Phrases (Median)
- [ ] Level Phrases (Mean / Loudest)
- [ ] Level with multi-selection only

### D. VST3 / Cubase
- [ ] Plug-in appears after rescan
- [ ] Editor opens
- [ ] Host playback not muted when plugin idle (pass-through)
- [ ] No crash on open/close editor

---

## Known limitations

- Undo is complete mainly for **Gain**
- Merge API exists but no dedicated UI button
- Auto Segment clears previous events by default
- Project stores path to audio, not embedded audio
- Full Cubase validation required on your machine

Vocal Editor — Avlirecordlabel
