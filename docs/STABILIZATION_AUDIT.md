# Vocal Editor Stabilization Audit

## Scope

This audit tracks the `stabilization/phase-1-audit` branch. It is a code review, not a claim that the application has passed runtime or Cubase validation.

## Confirmed findings from source inspection

### P0: Plug-in state is not persisted

`VocalEditorAudioProcessor::getStateInformation` and `setStateInformation` currently ignore their arguments. A host save/restore cycle therefore cannot restore the editor's internal state through these callbacks.

### P0: Shared state has data-race risk

`processBlock` accesses `currentlyPlaying`, `playbackSamplePosition`, `playbackPosition`, and `currentSampleRate` while UI/message-thread methods can read or write related state. These fields are not atomic or protected by a safe handoff. The processed-buffer snapshot mechanism does not make these other fields thread-safe.

### P1: Undo/Redo is exposed but edits are not comprehensively undoable

The processor exposes a `juce::UndoManager` and UI controls, but inspected edit methods directly mutate `VocalEvent` objects and do not register undoable actions. The current buttons must not be described as reliable edit history until every edit operation is connected and tested.

### P1: Envelope and event split need regression tests

`splitEvent` creates the right event with gain and fade metadata, but does not copy the original envelope points. A split of an event with a custom envelope can therefore lose the envelope shape on the right-hand side. The split operation also needs tests for fades, short regions, locked events, and selection state.

### P1: Auto-segmentation is threshold-based, not speech recognition

`autoSegmentFromSilence` calculates short-frame RMS and marks active regions separated by low-level frames. This is useful silence/activity segmentation, but does not identify words, syllables, phonemes, or breaths semantically. UI wording must remain honest about this distinction.

### P1: Processed-buffer handoff needs a concurrency audit

The three-buffer handoff uses atomic indexes, but the read/write protocol and fallback behavior when no target buffer is available need a formal stress test. The audio thread must never block, allocate, or read a buffer while the message thread is resizing/writing it.

### P2: Input file loading needs failure and bounds validation

`loadAudioFile` clears existing audio before confirming that a new reader can be created. A failed load can discard the current work in memory. Large-file bounds, reader failures, zero-length input, and sample-rate handling need explicit tests.

### P2: Build success is not behavioral verification

The project currently has no dedicated test target visible in the inspected CMake configuration. Add automated tests for event model and DSP behavior, then test the standalone app and VST3 in a host.

### Newly confirmed P1: source and host sample rates are conflated

`prepareToPlay()` assigns the host/device sample rate to `currentSampleRate`, while the same value is also used to interpret source-file sample indices, event times, fades, segmentation, and export. When the host rate differs from the imported file's native rate, transport timing and exported duration/sample rate can be wrong. The file-audition path currently copies samples directly instead of resampling. Next audio-engine task: separate source sample rate from host output rate, then implement real-time-safe resampling and regression tests before claiming reliable playback across rates.

## Phase 5 progress: source/host sample-rate separation

- [x] Keep the imported file's native sample rate separate from the host/device output sample rate.
- [x] Keep timeline duration, event boundaries, fades, segmentation, and export tied to the source file's native sample rate.
- [x] Map playback positions in seconds and resample source audio to the host output rate without allocating or taking a blocking lock in `processBlock`.
- [x] Extract sample-position mapping and linear interpolation into a small helper used by the playback path.
- [x] Add automated test cases for 44.1 kHz source to 48 kHz output mapping, time offsets, fractional interpolation, and invalid channel handling.
- [ ] Compile and run CTest on Windows; source changes have not been compiler-verified in this environment.
- [ ] Linear interpolation is a baseline, not a band-limited production resampler. Downsampling can alias; replace with a quality resampler and add frequency-response tests before release.
- [ ] Verify transport start/stop, seek, end-of-file, mono/stereo conversion, and host sample-rate changes in Standalone and Cubase.

## Phase 6 progress: audio processing edge cases

- [x] Extract fade math into a dedicated helper and make fade-in/fade-out reach exact silence at their final endpoint, including one-sample fades.
- [x] Fit overlapping fade lengths proportionally within very short events to prevent negative or out-of-range fade windows.
- [x] Add tests for fade endpoints, single-sample regions, and interval overlap semantics.
- [x] Reject new manual event regions that overlap existing events, because the current processor multiplies their gains if they overlap.
- [ ] Existing projects or Auto Segment may still contain overlaps; add project validation and Auto Segment overlap tests.
- [ ] Compile and run the test target. No test result is claimed yet.
- [ ] Review fade curve behavior at very short lengths and compare against a reference implementation.
- [ ] Add signal-level tests that render a known waveform and verify measured output RMS/peak against expected gain and fade values.

## Status clarification

Earlier sections describe findings from the initial audit snapshot. Since then, state serialization, safer staged import, event snapshot undo actions, processed audio export, and the sample-rate separation above have been added on this branch. The outstanding items in the validation checklist remain unverified until a real build and tests run.

## Recommended implementation order

1. Preserve this branch as a stabilization branch; do not modify the existing feature branch directly.
2. Add a test target and regression tests for the event model and current DSP behavior.
3. Fix the split/envelope data-loss issue with tests.
4. Implement safe processor state serialization and project-state validation.
5. Audit and correct UI/audio-thread communication without blocking the real-time thread.
6. Make edit operations undoable, then verify undo/redo across each operation.
7. Harden audio import/export and test failure paths.
8. Run the JUCE/VST3 validator where available and perform manual Cubase/Standalone checks.

## Phase 1 progress

- [x] Extract envelope slicing into a reusable `VocalEvent::makeEnvelopeSlice` helper.
- [x] Add regression tests for interpolated slice boundaries, split points at existing envelope points, flat envelopes, and invalid ranges.
- [x] Add a CTest target so the regression executable can be run with `ctest`.
- [x] Add a Windows GitHub Actions workflow to configure/build the plugin, standalone app, and tests.
- [ ] Confirm the Windows workflow passes. A source commit or an empty status response is not proof of a successful build.
- [ ] Add processor-level tests for `splitEvent` itself; current tests exercise the envelope slicing primitive directly.

Run locally from the project root after CMake configuration/build:

```powershell
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

## Phase 2 progress: persistence and safety

- [x] Add versioned XML state for audio source path, playback position, timeline selection, event metadata, envelope points, and selected event IDs.
- [x] Wire host `getStateInformation` / `setStateInformation` to the state serializer.
- [x] Wire `.vocalproj` project save/load methods and editor buttons, including Ctrl+S and Ctrl+Shift+O.
- [x] Validate project XML fields and stage source-audio decoding before replacing the active session.
- [x] Make failed audio imports preserve the current session and report the failure in the UI.
- [x] Replace the single reader-index handoff with per-buffer reader counters; make transport values and audio-availability status atomic.
- [ ] Build and run automated tests. These changes have been committed but are not yet compiler-verified in this environment.
- [ ] Project files currently reference the source audio by absolute path; moving the audio file can break relinking. Portable project media packaging is a later task.
- [ ] Review full audio-thread/UI-thread ownership of waveform and event model access; the current changes do not claim the entire application is race-free.

## Phase 3 progress: edit history

- [x] Add snapshot-based undo actions for create/delete, gain, fades, split, auto-segment, and Level Process.
- [x] Add multi-event deletion and a conservative merge operation for same-type events with a gap no larger than 5 ms.
- [x] Refresh waveform/timeline/selection UI after Undo/Redo.
- [x] Fix shortcut precedence so Ctrl+Shift+O opens a project instead of being caught by Ctrl+O first.
- [x] Add Delete/Backspace handling for selected events.
- [ ] Compile and execute tests; no successful build is claimed yet.
- [ ] Add processor-level automated tests for undo/redo ordering, envelope round trips, and locked-event behavior.
- [ ] Review undo transaction grouping for continuous slider drags and memory use on sessions with many events.
- [ ] Finish merge semantics and edge cases, including near-zero gaps and envelopes with missing endpoints.

## Phase 4 progress: processed audio export

- [x] Implement export of the processed audio snapshot to 24-bit WAV or AIFF.
- [x] Snapshot the published processed buffer through the reader-counter protocol before file writing.
- [x] Reject missing audio, unsupported extensions, invalid output paths, and attempts to overwrite the source file.
- [x] Add an Export Audio dialog and visible success/failure status.
- [ ] Build and test writer API compatibility, channel counts, sample rates, output metadata, and actual file readability.
- [ ] Add selectable bit depth/sample format and an explicit peak/clip report in a later pass.
- [ ] Add automated tests that compare exported samples against the processed snapshot.

## Validation checklist

- [ ] Configure and build Release on Windows with the documented Visual Studio/CMake setup.
- [ ] Load WAV/AIFF/FLAC and reject unsupported or invalid files without losing the current session.
- [ ] Verify Gain/Fade/Envelope output numerically and by listening.
- [ ] Split an event with a non-flat envelope and confirm both sides preserve the intended curve.
- [ ] Save/restore host state and confirm events/settings/audio reference behavior.
- [ ] Stress transport and edits while the audio callback is running; confirm no glitches or unsafe buffer access.
- [ ] Verify undo/redo for each supported edit.
- [ ] Validate VST3 loading and operation in Cubase, plus standalone playback/export.
