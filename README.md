# Vocal Editor

**A Product by Avlirecordlabel**

Professional Vocal Editing • Intelligent Analysis • Precision Control

## Phase 1 – Foundation

This repository contains the **Phase 1** implementation:

- Project structure with **JUCE 8** (CMake)
- Standalone + VST3 targets
- Modern dark UI (graphite / turquoise)
- Waveform display with `AudioThumbnail`
- Timeline / time ruler
- Mouse selection (click + drag)
- Zoom (mouse wheel + buttons)
- Scroll / view range
- Basic transport (Play / Stop)
- Drag & Drop + Open file dialog
- Playback of loaded audio

### Supported audio formats (Phase 1)
WAV, AIFF, FLAC, MP3, OGG (via JUCE)

---

## Requirements

- CMake ≥ 3.22
- C++17 compiler (MSVC, Clang, GCC)
- JUCE 8.x (clone next to the project)

## Building

### 1. Clone the project and JUCE

```bash
git clone https://github.com/alivalipourmusic/VocalEditor.git
cd VocalEditor

# Clone JUCE next to the project
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git ../JUCE
```

### 2. Configure & Build (Windows – Visual Studio recommended)

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### 3. Linux / macOS

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j$(nproc)
```

### Outputs
- Standalone: `VocalEditor_Standalone`
- VST3: `VocalEditor.vst3`

---

## Controls (Phase 1)

| Action              | How                                      |
|---------------------|------------------------------------------|
| Open file           | Open button or Drag & Drop               |
| Select region       | Click + drag on waveform                 |
| Extend selection    | Shift + click                            |
| Clear selection     | Double-click                             |
| Move playhead       | Click on waveform                        |
| Zoom                | Mouse wheel or +/- buttons               |
| Zoom to fit         | Fit button                               |
| Play / Pause        | Play button                              |
| Stop + rewind       | Stop button                              |

---

## Roadmap (from Product Spec)

| Phase | Focus                                      | Status     |
|-------|--------------------------------------------|------------|
| 1     | UI + Waveform + Timeline + Selection       | **Current** |
| 2     | Gain / Fade / Envelope / Split / Merge / Undo | Planned  |
| 3     | Full Standalone + VST3 stability           | Planned    |
| 4     | First AI features (Word detect + Smart Leveling) | Planned |
| 5     | Advanced AI + Dynamics                      | Planned   |
| 6     | Pitch editing + Release                    | Planned   |

---

## License

Proprietary – Avlirecordlabel  
All rights reserved.
