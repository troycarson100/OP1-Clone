#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>

// Orbit visualization component
// Shows 4 sample slots (A-D) in corners with orbit shape overlay
class OrbitVisualizationComponent : public juce::Component {
public:
    OrbitVisualizationComponent();
    ~OrbitVisualizationComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Set sample preview data for slots A-D
    void setSlotPreview(int slotIndex, const std::vector<float>& sampleData);
    
    // Set orbit weights (0.0-1.0 for each slot A-D)
    void setOrbitWeights(const std::array<float, 4>& weights);
    
    // Set orbit shape (0=Circle, 1=PingPong, 2=Corners, 3=RandomSmooth)
    void setOrbitShape(int shape);
    
    // Set orbit rate (Hz)
    void setOrbitRate(float rateHz);
    
    // Set orbit phase (0.0-1.0) for dot animation
    void setOrbitPhase(float phase);
    
    // Set which slots are loaded
    void setSlotLoaded(int slotIndex, bool loaded);
    
private:
    // Sample preview data for slots A-D (indices 0-3)
    std::array<std::vector<float>, 4> slotPreviewData;
    std::array<bool, 4> slotLoaded;  // Whether each slot has a sample loaded
    
    // Orbit state
    std::array<float, 4> orbitWeights;  // Current weights for slots A-D
    int orbitShape;  // 0=Circle, 1=PingPong, 2=Corners, 3=RandomSmooth, 4=Figure8, 5=ZigZag, 6=Spiral, 7=Square
    float orbitRateHz;
    float orbitPhase;  // Current phase (0.0-1.0) for dot animation
    
    // Drawing helpers
    void drawSlotInCorner(juce::Graphics& g, int slotIndex, const juce::Rectangle<int>& bounds);
    void drawOrbitShape(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawWaveformPreview(juce::Graphics& g, const juce::Rectangle<int>& bounds, const std::vector<float>& data, float weight);
    
    // Get corner rectangle for a slot (0-3 for A-D)
    juce::Rectangle<int> getCornerBounds(int slotIndex, const juce::Rectangle<int>& fullBounds);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitVisualizationComponent)
};

