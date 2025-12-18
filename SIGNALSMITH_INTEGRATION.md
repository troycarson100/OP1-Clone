# Signalsmith Stretch Integration

## Status

Integration structure is complete. The library files need to be vendored before compilation.

## What's Done

1. ✅ Created portable `ITimePitch` interface in `/Core/ITimePitch.h`
2. ✅ Created `SignalsmithTimePitch` wrapper in `/Core/SignalsmithTimePitch.h/.cpp`
3. ✅ Created `/ThirdParty/signalsmith/` directory structure
4. ✅ Updated `SamplerVoice` to use `ITimePitch` interface
5. ✅ Updated `CMakeLists.txt` to include new files and ThirdParty directory
6. ⚠️ Debug UI - needs completion (see below)

## Next Steps

### 1. Vendor Signalsmith Stretch Library

```bash
cd ThirdParty/signalsmith
git clone https://github.com/Signalsmith-Audio/signalsmith-stretch.git .
# Or download and extract the library files
```

Required files:
- `signalsmith-stretch.h` (main header)
- Signalsmith Linear headers (as per upstream README)
- MIT license file(s)

### 2. Verify API Compatibility

The `SignalsmithTimePitch.cpp` implementation assumes:
- `presetDefault(channels, sampleRate, splitComputation)` method
- `setTransposeSemitones(semitones)` method
- `process(inputBuffers, inputSamples, outputBuffers, outputSamples)` method
- `inputLatency()` and `outputLatency()` methods

If the actual API differs, adjust `SignalsmithTimePitch.cpp` accordingly.

### 3. Complete Debug UI

Add debug overlay in `PluginEditor` showing:
- Selected MIDI note (from last note-on)
- Pitch semitones (current note - root note)
- inSamples/outSamples per block (atomic counters)
- inputLatency/outputLatency (from ITimePitch)

Implementation:
1. Add atomic variables in `PluginProcessor` to track debug info
2. Update atomics in audio thread (safe, lock-free)
3. Read atomics in UI thread (via timer) and display in labels

### 4. Test Constant Duration

After library is integrated:
1. Play notes across keyboard (C3, C4, C5, etc.)
2. Verify all notes play for same duration
3. Verify pitch changes correctly
4. Check for artifacts or quality issues

## Known Limitations

- Library files not yet vendored (compilation will fail until added)
- Debug UI not yet implemented
- Flush handling may need adjustment based on actual Signalsmith API
- Buffer format (interleaved vs non-interleaved) may need verification

## Embedded Hardware Porting

See `EMBEDDED_PORTING_NOTES.md` for details on replacing Signalsmith with embedded-friendly implementation.

