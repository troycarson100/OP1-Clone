#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "JuceVisualizationRenderer.h"
#include <memory>

// Pill-shaped ADSR visualization component
// Uses IVisualizationRenderer interface - no direct JUCE Graphics usage
// Always visible in bottom right corner above paramDisplay4
class ADSRPillComponent : public juce::Component {
public:
    ADSRPillComponent();
    ~ADSRPillComponent() override;
    
    void paint(juce::Graphics& g) override;
    
    // Set ADSR parameters (in milliseconds, except sustain which is 0.0-1.0)
    void setADSR(float attackMs, float decayMs, float sustain, float releaseMs);
    
private:
    // Renderer instance (JUCE implementation)
    std::unique_ptr<JuceVisualizationRenderer> renderer;
    
    float attackMs;
    float decayMs;
    float sustainLevel;  // 0.0 to 1.0
    float releaseMs;
    
    // Build Core::ADSRData from current state
    Core::ADSRData buildADSRData() const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ADSRPillComponent)
};
