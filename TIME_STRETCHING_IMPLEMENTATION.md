# Time-Stretching Pitch Shifter Implementation

## Overview

This document describes the Phase Vocoder-based pitch shifter with time-scale modification (TSM) implementation for the OP-1 Clone sampler. The implementation ensures that all notes play at the same duration regardless of pitch, achieving true time-stretching.

## Architecture

### Core Modules

1. **WindowFunctions** (`WindowFunctions.h/.cpp`)
   - Generates Hann windows for STFT analysis/synthesis
   - Normalized for perfect reconstruction with 75% overlap

2. **CircularBuffer** (`CircularBuffer.h/.cpp`)
   - Fixed-size circular buffer for audio processing
   - No dynamic allocations during runtime
   - Thread-safe for single producer/consumer

3. **SimpleFFT** (`SimpleFFT.h/.cpp`)
   - Minimal radix-2 FFT implementation
   - Supports power-of-2 frame sizes (256, 512, 1024, 2048)
   - Real-to-complex forward FFT and complex-to-real inverse FFT

4. **STFT** (`STFT.h/.cpp`)
   - Short-Time Fourier Transform wrapper
   - Handles windowing, FFT, and IFFT
   - Frame size: 1024 samples
   - Hop size: 256 samples (75% overlap)

5. **PitchShiftTSM** (`PitchShiftTSM.h/.cpp`)
   - Phase Vocoder pitch shifter
   - Implements Bernsee's approach for pitch shifting
   - Maintains constant duration via equal analysis/synthesis hop sizes

## Algorithm Details

### Phase Vocoder Principle

The phase vocoder works by:
1. **Analysis**: Break input into overlapping frames, apply window, compute FFT
2. **Phase Manipulation**: Extract magnitude and phase, unwrap phase differences, scale phase advances by pitch ratio
3. **Synthesis**: Reconstruct complex spectrum with modified phases, IFFT, apply window, overlap-add

### Why Constant Duration?

- **Analysis hop (Ha) = Synthesis hop (Hs)**: When both hops are equal, the time-scale factor is 1.0
- **Pitch is changed by scaling phase advances**, not by changing playback speed
- **Result**: Same duration for all notes, different pitches

### Frame/Hop Size Choices

- **Frame size: 1024 samples**
  - Larger = better frequency resolution, smoother pitch shifts, more CPU, more latency
  - 1024 is a good balance for real-time performance

- **Hop size: 256 samples (frameSize/4)**
  - 75% overlap is standard for phase vocoder
  - Smaller hop = better time resolution, more CPU (more frames to process)
  - 256 provides good quality with reasonable CPU usage

### Quality vs CPU Tradeoffs

- **Larger frame size**: Better quality, more CPU, more latency
- **Smaller hop**: Better time resolution, more CPU
- **Phase locking**: Optional improvement for transients (not yet implemented)

## Integration

### SamplerVoice Integration

The `SamplerVoice` class now:
1. Uses `PitchShiftTSM` instead of direct resampling
2. Feeds sample data through the pitch shifter block-by-block
3. Applies envelope (attack/release) to the pitch-shifted output
4. Maintains constant playback speed (time-stretching)

### Latency

- **Latency**: ~768 samples (frameSize - hopSize = 1024 - 256)
- **At 44.1kHz**: ~17.4ms latency
- This is acceptable for a sampler but noticeable on very short samples

## Known Limitations

1. **Phasiness**: 
   - Phase vocoder can introduce a "phasy" or "chorus-like" artifact, especially on transients
   - Mitigated by simple transient detection that reduces phase propagation near transients
   - Can be improved with phase locking (not yet implemented)

2. **Latency**:
   - ~17ms latency due to frame size and overlap
   - May be noticeable on very short samples or fast playing
   - Can be reduced by using smaller frame size (512) at cost of quality

3. **CPU Usage**:
   - Phase vocoder is more CPU-intensive than simple resampling
   - FFT operations are the main cost
   - Should be fine for moderate polyphony on modern hardware

4. **Memory**:
   - Pre-allocated buffers for FFT, windows, phase history
   - ~10-20KB per voice (acceptable)

5. **Extreme Pitch Ratios**:
   - Very high (>2.0) or very low (<0.5) pitch ratios may show more artifacts
   - Current implementation clamps to [0.25, 4.0] range

## Future Improvements

1. **Phase Locking**: Improve transient handling by locking phases of related frequency bins
2. **Adaptive Frame Size**: Use smaller frames for transients, larger for steady tones
3. **Formant Preservation**: Maintain formant structure for more natural vocal/instrument sounds
4. **Zero-Padding**: Reduce latency by using zero-padding techniques

## Testing

To test the time-stretching:
1. Load a sample (WAV file)
2. Play different MIDI notes (e.g., C3, C4, C5)
3. All notes should have the same duration but different pitches
4. Listen for artifacts (phasiness, latency, quality issues)

## File Sizes

All files are kept under 500 lines:
- `WindowFunctions.cpp`: ~60 lines
- `CircularBuffer.cpp`: ~120 lines
- `SimpleFFT.cpp`: ~200 lines
- `STFT.cpp`: ~80 lines
- `PitchShiftTSM.cpp`: ~250 lines
- `SamplerVoice.cpp`: ~220 lines (updated)

## References

- Bernsee, S. M. "Pitch Shifting Using The Fourier Transform" - Classic phase vocoder tutorial
- Phase vocoder algorithm based on standard STFT approach
- Implementation is clean, portable C++ suitable for hardware porting

