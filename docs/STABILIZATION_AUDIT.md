# Vocal Editor Stabilization Audit

## Scope

This audit is based on the current `feature/real-gain-fade` source snapshot. It is a code review, not a claim that the application has passed runtime or Cubase validation.

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

## Recommended implementation order

1. Preserve this branch as a stabilization branch; do not modify the existing feature branch directly.
2. Add a test target and regression tests for the event model and current DSP behavior.
3. Fix the split/envelope data-loss issue with tests.
4. Implement safe processor state serialization and project-state validation.
5. Audit and correct UI/audio-thread communication without blocking the real-time thread.
6. Make edit operations undoable, then verify undo/redo across each operation.
7. Harden audio import/export and test failure paths.
8. Run the JUCE/VST3 validator where available and perform manual Cubase/Standalone checks.

## Validation checklist

- [ ] Configure and build Release on Windows with the documented Visual Studio/CMake setup.
- [ ] Load WAV/AIFF/FLAC and reject unsupported or invalid files without losing the current session.
- [ ] Verify Gain/Fade/Envelope output numerically and by listening.
- [ ] Split an event with a non-flat envelope and confirm both sides preserve the intended curve.
- [ ] Save/restore host state and confirm events/settings/audio reference behavior.
- [ ] Stress transport and edits while the audio callback is running; confirm no glitches or unsafe buffer access.
- [ ] Verify undo/redo for each supported edit.
- [ ] Validate VST3 loading and operation in Cubase, plus standalone playback/export.
