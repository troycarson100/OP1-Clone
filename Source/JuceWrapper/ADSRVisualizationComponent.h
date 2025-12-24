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
    
    // Override visibility to respect paintingEnabled flag
    // Note: isVisible() is not virtual in JUCE Component, but we provide our own implementation
    bool isVisible() const;
    
    // Override setBounds to prevent bounds from being set unless painting is enabled
    // Note: setBounds() is not virtual in JUCE Component, but we provide our own implementation
    void setBounds(int x, int y, int width, int height);
    void setBounds(const juce::Rectangle<int>& bounds);
    
    // Set ADSR parameters (in milliseconds, except sustain which is 0.0-1.0)
    void setADSR(float attackMs, float decayMs, float sustain, float releaseMs);
    
    // Set maximum time range for display (in milliseconds)
    void setMaxTimeRange(float maxMs) { maxTimeMs = maxMs; repaint(); }
    
    // Set alpha/opacity (0.0 to 1.0)
    void setAlpha(float alpha) { 
        currentAlpha = juce::jlimit(0.0f, 1.0f, alpha); 
        // Only repaint if actually visible, in component tree, and has valid bounds
        if (currentAlpha > 0.0f && isVisible() && getParentComponent() != nullptr) {
            auto b = getBounds();
            if (b.getWidth() > 0 && b.getHeight() > 0) {
                repaint();
            }
        }
    }
    
    // Flag to explicitly enable/disable painting
    void setPaintingEnabled(bool enabled) { paintingEnabled = enabled; }
    bool isPaintingEnabled() const { return paintingEnabled; }
    
    float getAlpha() const { return currentAlpha; }
    
private:
    float attackMs;
    float decayMs;
    float sustainLevel;  // 0.0 to 1.0
    float releaseMs;
    float maxTimeMs;  // Maximum time range to display (default 1000ms = 1 second)
    float currentAlpha;  // Current alpha/opacity (0.0 to 1.0)
    bool paintingEnabled;  // Flag to explicitly enable/disable painting
    
    // Helper: Convert time (ms) to x coordinate
    float timeToX(float timeMs, float width) const;
    
    // Helper: Convert amplitude (0.0-1.0) to y coordinate (inverted, top is 1.0)
    float amplitudeToY(float amplitude, float height) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ADSRVisualizationComponent)
};

