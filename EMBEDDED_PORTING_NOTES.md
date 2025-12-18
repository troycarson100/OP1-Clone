# Embedded Hardware Porting Notes

## Signalsmith Stretch Integration

The current implementation uses Signalsmith Stretch library for high-quality time-stretching and pitch-shifting. This is a temporary solution for desktop development.

## Porting to Embedded Hardware

### 1. Replace SignalsmithTimePitch Implementation

Create a new implementation of `Core::ITimePitch` interface that:
- Does not depend on Signalsmith Stretch
- Does not use Accelerate/SIMD (or provides fallback)
- Uses hardware-specific or simpler FFT implementation
- Confirms no runtime allocations (all buffers pre-allocated in `prepare()`)

### 2. Key Considerations

**FFT Requirements:**
- Signalsmith Stretch uses Signalsmith Linear for FFT operations
- For embedded: replace with hardware-specific FFT (e.g., CMSIS-DSP for ARM) or simpler radix-2 FFT
- Ensure FFT size matches or is compatible with time-stretching algorithm

**SIMD/Accelerate:**
- Signalsmith may use platform-specific SIMD optimizations
- For embedded: either disable SIMD or provide hardware-specific SIMD (e.g., ARM NEON)
- Test performance impact of disabling SIMD

**Memory:**
- All buffers must be pre-allocated in `prepare()` method
- No dynamic allocations in `process()` method
- Consider fixed-size buffers suitable for embedded constraints

**Real-time Safety:**
- No locks/mutexes in audio thread
- No file I/O in audio thread
- All operations must complete within block time

### 3. Interface Compliance

The `ITimePitch` interface is designed to be hardware-portable:
- `prepare()`: Allocate all buffers here
- `process()`: Process audio with pre-allocated buffers
- `reset()`: Reset state without allocations
- `setPitchSemitones()` / `setTimeRatio()`: Update parameters atomically

### 4. Testing Strategy

1. **Desktop Testing:**
   - Verify Signalsmith implementation works correctly
   - Test constant duration across different pitches
   - Measure latency and CPU usage

2. **Embedded Preparation:**
   - Create stub implementation of `ITimePitch` that passes through audio
   - Verify interface compiles and links on embedded toolchain
   - Gradually implement time-stretching algorithm

3. **Algorithm Options:**
   - WSOLA (Waveform Similarity Overlap-Add) - simpler, lower quality
   - Phase Vocoder - higher quality, more CPU intensive
   - Granular synthesis - good balance, hardware-friendly
   - Hardware-specific DSP algorithms (if available)

### 5. Files to Modify

- `Source/Core/SignalsmithTimePitch.h/.cpp`: Replace with embedded implementation
- `Source/Core/SamplerVoice.cpp`: No changes needed (uses `ITimePitch` interface)
- `CMakeLists.txt`: Remove Signalsmith dependencies, add embedded-specific includes

### 6. Example Embedded Implementation Structure

```cpp
// Source/Core/EmbeddedTimePitch.h
class EmbeddedTimePitch : public ITimePitch {
    // Use hardware-friendly algorithm (e.g., WSOLA or granular)
    // Pre-allocate all buffers in prepare()
    // Use fixed-size buffers suitable for embedded constraints
};
```

### 7. Performance Targets

- CPU usage: < 20% on target embedded platform
- Latency: < 50ms total (input + output)
- Memory: < 64KB for time-stretching buffers
- Quality: Acceptable for music production (may be lower than Signalsmith)

