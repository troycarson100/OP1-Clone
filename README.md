# OP-1 Clone Sampler - Step 0-1

A hardware-portable sampler engine with JUCE wrapper for macOS testing.

## Architecture

- **Core/** - Portable C++ engine (no JUCE dependencies)
- **JuceWrapper/** - Thin adapter layer between JUCE and Core

## Getting Started

### First Time Setup

1. **Clone JUCE** (required for building):
   ```bash
   git clone https://github.com/juce-framework/JUCE.git JUCE
   ```
   
   Or if you want to use it as a submodule:
   ```bash
   git submodule add https://github.com/juce-framework/JUCE.git JUCE
   git submodule update --init --recursive
   ```

## Building

### Prerequisites
- CMake 3.22 or later
- JUCE framework (version 7.0+)
- Xcode (for macOS builds)
- C++17 compatible compiler

### Build Steps

#### Option 1: Using JUCE as Git Submodule (Recommended)

1. **Clone JUCE as submodule** (if not already done):
   ```bash
   git submodule add https://github.com/juce-framework/JUCE.git JUCE
   git submodule update --init --recursive
   ```

2. **Configure CMake**:
   ```bash
   mkdir build
   cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

3. **Build**:
   ```bash
   cmake --build . --config Release
   ```

4. **Install plugin** (macOS):
   ```bash
   cmake --install . --config Release
   ```
   
   This will install the AU and VST3 plugins to:
   - AU: `~/Library/Audio/Plug-Ins/Components/Op1Clone.component`
   - VST3: `~/Library/Audio/Plug-Ins/VST3/Op1Clone.vst3`

#### Option 2: Using Projucer (Alternative)

If you prefer using Projucer:
1. Open Projucer
2. Create a new Audio Plugin project
3. Copy the source files from `Source/` into your Projucer project
4. Configure plugin settings (AU, VST3, Standalone)
5. Save and open in Xcode
6. Build from Xcode

**Note**: The CMakeLists.txt assumes JUCE is in a `JUCE/` subdirectory. If you have JUCE installed elsewhere, modify the `add_subdirectory(JUCE)` line or set `JUCE_DIR`.

## Testing

### Quick Test in DAW (Ableton Live)

1. **Load the plugin**:
   - Open Ableton Live
   - Create a new MIDI track
   - Add "Op1Clone" as an instrument

2. **Trigger playback**:
   - Play MIDI note **C4 (note 60)** on your keyboard or piano roll
   - You should hear a 440Hz sine wave tone (1 second duration)

3. **Test note-off**:
   - Release the key - playback should stop immediately

4. **Adjust gain**:
   - Use the gain slider in the plugin UI to adjust volume

### Testing in JUCE Audio Plugin Host

1. Build the "Standalone" target
2. Run the standalone app
3. Send MIDI note 60 to trigger playback

## Current Implementation

### Features
- ✅ Mono sample playback (duplicated to stereo)
- ✅ MIDI note-on triggers playback
- ✅ MIDI note-off stops playback
- ✅ Basic gain parameter with smoothing
- ✅ Portable core engine (no JUCE dependencies)

### Default Sample
Currently uses a generated 440Hz sine wave (1 second) as the test sample. This is loaded automatically on first `prepareToPlay()`.

### MIDI Note
- **Note 60 (C4)** triggers playback

## Known Limitations (Step 0-1)

- Single voice only (no polyphony)
- No time-stretching or pitch shifting
- No looping or envelopes
- No preset/state saving (beyond basic parameters)
- Simple hard stop on note-off (no release envelope)
- Default sample is generated tone (not loaded from file)
- No UI beyond basic gain slider

## Next Steps

- Load actual WAV files from BinaryData
- Add polyphony (multiple voices)
- Add ADSR envelope
- Add looping support
- Add time-stretching/pitch shifting

## Project Rules

- All files ≤ 500 lines
- **JUCE is ONLY a wrapper** - See `PROJECT_RULES.md` for the permanent rule
- **Core logic must be JUCE-agnostic** - See `Source/Core/CORE_RULES.md` for details
- **Visualization uses abstract interfaces** - No direct JUCE Graphics usage
- No allocations in audio thread
- Portable C++ only in Core/ (NO JUCE dependencies allowed)

### Verifying Core Purity

Before committing, verify Core has no JUCE dependencies:

```bash
./verify_core_purity.sh
```

This script checks that `Source/Core/` contains **zero** JUCE dependencies. See `Source/Core/CORE_RULES.md` for full guidelines.

### Visualization Architecture

All visualization uses abstract interfaces:
- `Core/IVisualizationRenderer.h` - Pure C++ interface
- `JuceWrapper/JuceVisualizationRenderer.cpp` - JUCE implementation (ONLY place JUCE Graphics is used)
- Components use the interface, not JUCE Graphics directly

See `PROJECT_RULES.md` for the complete architecture and rules.

