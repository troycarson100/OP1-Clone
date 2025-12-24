# Project Rules - Permanent

## CRITICAL RULE: JUCE is ONLY a Wrapper

**JUCE must NEVER be used for anything except as a wrapper/adapter layer.**

### What This Means

1. **Core Logic**: 100% pure C++ - NO JUCE dependencies
2. **Visualization**: Uses abstract interfaces - NO direct JUCE Graphics
3. **Audio Processing**: Pure C++ - NO JUCE DSP
4. **Data Structures**: Pure C++ - NO JUCE types

### Architecture

```
Source/
  Core/                    # 100% Pure C++ - NO JUCE
    - All audio processing
    - All DSP algorithms
    - All filters and effects
    - Abstract interfaces (IVisualizationRenderer, etc.)
    - Pure C++ data structures
    
  JuceWrapper/            # JUCE ONLY for wrapping/testing
    - Plugin hosting (JUCE AudioProcessor)
    - UI framework (JUCE Component)
    - Adapters (convert JUCE ↔ Core)
    - Implementations of Core interfaces using JUCE
```

### Rules

#### ✅ ALLOWED in JuceWrapper:
- JUCE for plugin hosting (`juce::AudioProcessor`, etc.)
- JUCE for UI framework (`juce::Component`, `juce::Graphics` for rendering)
- JUCE implementations of Core interfaces
- Adapters that convert between JUCE and Core types

#### ❌ FORBIDDEN Everywhere:
- Direct JUCE usage in Core logic
- JUCE types in Core data structures
- JUCE Graphics calls outside of renderer implementations
- JUCE DSP in Core audio processing

### Visualization Pattern

All visualization uses the abstract interface pattern:

```cpp
// Core/IVisualizationRenderer.h (pure C++ interface)
namespace Core {
    class IVisualizationRenderer {
        virtual void renderWaveform(const WaveformData&, const Rectangle&) = 0;
        virtual void renderADSR(const ADSRData&, const Rectangle&) = 0;
    };
}

// JuceWrapper/JuceVisualizationRenderer.cpp (JUCE implementation)
class JuceVisualizationRenderer : public Core::IVisualizationRenderer {
    void renderWaveform(...) override {
        // Use juce::Graphics here - this is the ONLY place
    }
};

// JuceWrapper/WaveformComponent.cpp (uses interface, not JUCE directly)
void WaveformComponent::paint(juce::Graphics& g) {
    renderer->setGraphicsContext(&g);
    renderer->renderWaveform(data, bounds);  // Calls through interface
}
```

### Enforcement

1. **Core Directory**: Run `./verify_core_purity.sh` before committing
2. **Visualization**: All rendering must go through `IVisualizationRenderer`
3. **Code Review**: Check that JUCE is only in JuceWrapper and only for wrapping
4. **CI/CD**: Add automated checks if possible

### Why This Rule Exists

- **Hardware Portability**: Core code must run on embedded systems
- **Separation of Concerns**: Core = logic, JuceWrapper = testing/UI
- **Future-Proofing**: Easy to port to any platform
- **Clean Architecture**: Clear boundaries between layers

### Questions?

If you're unsure:
1. Check this document
2. Check `Source/Core/CORE_RULES.md` for Core-specific rules
3. When in doubt: **Use pure C++ or move to JuceWrapper**

---

**Remember: JUCE = Wrapper Only, Core = Pure C++**

