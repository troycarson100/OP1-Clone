#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Pill-shaped ADSR visualization component
// Always visible in bottom right corner above paramDisplay4
class ADSRPillComponent : public juce::Component {
public:
    ADSRPillComponent();
    ~ADSRPillComponent() override;
    
    void paint(juce::Graphics& g) override;
    
    // Set ADSR parameters (in milliseconds, except sustain which is 0.0-1.0)
    void setADSR(float attackMs, float decayMs, float sustain, float releaseMs);
    
private:
    float attackMs;
    float decayMs;
    float sustainLevel;  // 0.0 to 1.0
    float releaseMs;
    
    // Helper: Convert time (ms) to x coordinate within pill bounds
    float timeToX(float timeMs, float width) const;
    
    // Helper: Convert amplitude (0.0-1.0) to y coordinate (inverted, top is 1.0)
    float amplitudeToY(float amplitude, float height) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ADSRPillComponent)
};

