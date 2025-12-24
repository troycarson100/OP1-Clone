# Core Module Rules

## CRITICAL: Pure C++ Only - NO JUCE Dependencies

The `Source/Core/` directory contains **ALL** audio processing logic and must be **100% pure C++** with **ZERO JUCE dependencies**.

### Why?
- **Hardware Portability**: Core code must run on embedded hardware without JUCE
- **Separation of Concerns**: Core = audio processing, JuceWrapper = UI/testing only
- **Future-Proofing**: Easy to port to any platform (DSP chips, embedded systems, etc.)

## Rules

### ✅ ALLOWED in Core:
- Standard C++ (C++11 or later)
- Standard library (`<vector>`, `<algorithm>`, `<cmath>`, etc.)
- Third-party libraries that are pure C++ (e.g., Signalsmith Stretch)
- Custom pure C++ implementations

### ❌ FORBIDDEN in Core:
- **ANY** JUCE includes (`#include <juce_*>`)
- **ANY** JUCE types (`juce::*`)
- **ANY** JUCE dependencies

### ✅ ALLOWED in JuceWrapper:
- JUCE for UI, graphics, plugin hosting
- JUCE for testing and development
- Adapters that convert between JUCE and Core types

## File Structure

```
Source/
  Core/              ← 100% Pure C++ (NO JUCE)
    - Audio processing
    - DSP algorithms
    - Filters
    - Envelopes
    - Sample handling
    - MIDI processing
    
  JuceWrapper/       ← JUCE allowed here (UI/testing only)
    - Plugin editor
    - UI components
    - JUCE adapters
    - Visualization (can use JUCE Graphics)
```

## How to Verify

### Before Committing:
1. **Search for JUCE in Core:**
   ```bash
   grep -r "juce::\|#include.*juce" Source/Core/
   ```
   This should return **ZERO results**.

2. **Check includes:**
   ```bash
   grep -r "#include" Source/Core/*.cpp Source/Core/*.h | grep -i juce
   ```
   This should return **ZERO results**.

### Automated Check (Optional):
You can add a pre-commit hook or CI check that runs:
```bash
if grep -r "juce::\|#include.*juce" Source/Core/; then
    echo "ERROR: JUCE dependencies found in Core!"
    exit 1
fi
```

## Examples

### ✅ GOOD (Pure C++):
```cpp
#include <vector>
#include <cmath>
#include <algorithm>

namespace Core {
    class MyFilter {
        float process(float input) {
            return input * 0.5f;
        }
    };
}
```

### ❌ BAD (JUCE dependency):
```cpp
#include <juce_dsp/juce_dsp.h>  // ❌ FORBIDDEN

namespace Core {
    class MyFilter {
        juce::dsp::Filter filter;  // ❌ FORBIDDEN
    };
}
```

## If You Need JUCE Functionality

1. **Implement it in pure C++** (preferred)
2. **Use a pure C++ third-party library**
3. **Create an abstract interface in Core**, implement with JUCE in JuceWrapper

Example of #3:
```cpp
// Core/IVisualizationRenderer.h (pure C++ interface)
namespace Core {
    class IVisualizationRenderer {
        virtual void renderWaveform(const WaveformData&) = 0;
    };
}

// JuceWrapper/JuceVisualizationRenderer.cpp (JUCE implementation)
class JuceVisualizationRenderer : public Core::IVisualizationRenderer {
    void renderWaveform(const Core::WaveformData& data) override {
        // Use juce::Graphics here
    }
};
```

## Enforcement

- **Code Review**: Always check Core files for JUCE dependencies
- **Pre-commit Hook**: Consider adding automated check (see above)
- **CI/CD**: Add check to build pipeline if possible

## Questions?

If you're unsure whether something is allowed:
1. Check this document
2. Search existing Core code for similar patterns
3. When in doubt, **implement in pure C++** or move to JuceWrapper

---

**Remember: Core = Pure C++, JuceWrapper = JUCE allowed**

