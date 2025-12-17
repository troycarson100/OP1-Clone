---
alwaysApply: true
---

## JUCE AS WRAPPER ONLY (HARDWARE-FIRST RULE)

JUCE is used **ONLY** as a temporary desktop wrapper for testing on macOS.

All core logic must be written as **JUCE-agnostic, portable C++** suitable for future embedded hardware targets.

---

## STRICT SEPARATION REQUIREMENT

### JUCE IS ALLOWED ONLY IN:
- `JuceAudioProcessor`
- `JuceAudioProcessorEditor`
- Thin adapter / bridge files (e.g. `JuceDSPAdapter.cpp`)
- UI components strictly for testing

### JUCE IS FORBIDDEN IN:
- Core DSP logic
- Sampler engines
- Sequencers
- Voice handling
- Timing systems
- State machines
- Audio buffers
- MIDI/event handling logic
- File formats (WAV parsing, etc.)

Core logic must **never** include:
```cpp
#include <juce_*.h>

PORTABLE CORE ARCHITECTURE (MANDATORY)

All non-UI logic must live in a /Core or /Engine folder and:

Use only standard C++ (STL allowed carefully)

Avoid OS-specific APIs

Avoid dynamic allocation during runtime

Avoid exceptions (optional but recommended)

Avoid RTTI where possible

JUCE should adapt to the core, not the other way around.

AUDIO & BUFFER RULES

Core DSP must operate on:

raw pointers (float*, const float*)

fixed-size buffers

simple structs

No juce::AudioBuffer, juce::dsp, or juce::SmoothedValue inside core logic

If smoothing is needed, implement a simple portable smoothing class

JUCE may convert:
juce::AudioBuffer<float> → float**
at the boundary layer only.

TIMING & THREADING RULES

Core logic must not depend on:

JUCE timers

JUCE message thread

JUCE thread classes

Core timing must be sample-accurate or block-based and hardware-portable

Any threading primitives must be replaceable with embedded equivalents

FILE I/O & SAMPLING RULES

Core sampler must assume samples are already in memory

No JUCE file pickers, file streams, or WAV readers in core

Desktop loading is handled by JUCE wrapper → raw buffers passed in

This mirrors real hardware behavior.

MIDI / INPUT ABSTRACTION

Core logic must consume generic events, not JUCE MIDI types

Example:
struct NoteEvent {
  int note;
  float velocity;
  bool on;
};

JUCE converts juce::MidiMessage → NoteEvent.

FORBIDDEN JUCE / DESKTOP FEATURES

Cursor must NOT use:

juce::dsp module

juce::Synthesiser

juce::AudioTransportSource

juce::Sampler

juce::SmoothedValue

juce::ValueTree inside core

Any plugin-specific helpers that won’t exist on hardware

ADAPTER PATTERN (REQUIRED)

If JUCE functionality is needed:

Write a thin adapter layer

Keep the core unaware of JUCE

Ensure adapters can be swapped later for hardware drivers

DECISION RULE

When choosing between:

“JUCE has a helper for this”

“Write a small portable C++ version”

Cursor must always choose portable C++.

GOAL STATEMENT

The MVP must be runnable on macOS via JUCE
AND realistically portable to embedded hardware with minimal rewrite.

JUCE exists only to visualize, test, and debug.

---

### Why this rule is 🔥
- Prevents accidental JUCE lock-in
- Forces hardware-friendly architecture
- Makes porting to STM32 / Daisy / Teensy / custom ARM realistic
- Keeps Cursor from “cheating” with JUCE magic

---

### Next Step (Recommended)

Before we write **any code**, I strongly recommend we do **Step 0**:

**Step 0 – Project Skeleton**
- Folder structure
- Core vs JUCE boundary
- Empty portable engine classes
- No DSP yet

Then:
1. Test build
2. Add sampler voice
3. Test
4. Add sequencing
5. Test
6. Add UI scaffolding

If you want, say:
> **“Let’s do Step 0.”**

I’ll keep every step small, testable, and hardware-safe.
