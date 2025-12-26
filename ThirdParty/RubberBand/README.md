# Rubber Band Library Integration

## License Notice

**IMPORTANT**: Rubber Band Library is licensed under the **GPL v2 or later**, unless you have acquired a commercial license from the copyright holders.

If you are using this in a commercial product without a commercial license, you must:
1. Release your entire application under GPL v2 or later
2. Provide source code to all users
3. Include the GPL license text

For commercial licensing, contact: https://breakfastquay.com/rubberband/

## Installation

### Option 1: System Installation (macOS with Homebrew)

```bash
brew install rubberband
```

### Option 2: Build from Source

1. Clone the Rubber Band repository:
```bash
cd ThirdParty
git clone https://github.com/breakfastquay/rubberband.git
cd rubberband
```

2. Build the library:
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

3. Install (optional, for system-wide use):
```bash
sudo make install
```

### Option 3: Vendor in Project

1. Copy the Rubber Band source into `ThirdParty/RubberBand/`
2. Update `CMakeLists.txt` to build Rubber Band as a subdirectory

## Build Configuration

To enable Rubber Band backend, configure CMake with:

```bash
cmake -DUSE_RUBBERBAND=ON ..
```

Or set it in CMakeLists.txt:
```cmake
option(USE_RUBBERBAND "Enable Rubber Band Library for time-stretching" ON)
```

If `USE_RUBBERBAND` is OFF or Rubber Band is not found, the build will use `WarpBackendNone` (passthrough).

## Dependencies

Rubber Band requires:
- FFT library (FFTW, Accelerate, or similar)
- Resampler library (libsamplerate or similar)

These are typically provided by the system or bundled with Rubber Band.

## Usage

The Rubber Band backend is automatically selected when `USE_RUBBERBAND` is enabled. The interface (`IWarpBackend`) abstracts the implementation, so no code changes are needed beyond the build configuration.

## Testing

After building with Rubber Band enabled:
1. Test warp ON/OFF toggling (should be click-free)
2. Test rapid retrigger chords (no pops)
3. Test mid-note warp toggle (should crossfade smoothly)
4. Check debug metrics: `warpPeak`, `warpMaxDelta`, `underrunCount`, `availableFrames`



