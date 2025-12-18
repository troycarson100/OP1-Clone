#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Component that draws an ADSR envelope visualization
// Overlays on the screen to show envelope shape
class ADSRVisualizationComponent : public juce::Component {
public:
    ADSRVisualizationComponent();
    ~ADSRVisualizationComponent() override;
    
    void paint(juce::Graphics& g) override;
    
    // Set ADSR parameters (in milliseconds, except sustain which is 0.0-1.0)
    void setADSR(float attackMs, float decayMs, float sustain, float releaseMs);
    
    // Set maximum time range for display (in milliseconds)
    void setMaxTimeRange(float maxMs) { maxTimeMs = maxMs; repaint(); }
    
private:
    float attackMs;
    float decayMs;
    float sustainLevel;  // 0.0 to 1.0
    float releaseMs;
    float maxTimeMs;  // Maximum time range to display (default 1000ms = 1 second)
    
    // Helper: Convert time (ms) to x coordinate
    float timeToX(float timeMs, float width) const;
    
    // Helper: Convert amplitude (0.0-1.0) to y coordinate (inverted, top is 1.0)
    float amplitudeToY(float amplitude, float height) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ADSRVisualizationComponent)
};

