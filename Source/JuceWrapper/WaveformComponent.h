#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// OP-1 style waveform visualizer
class WaveformComponent : public juce::Component {
public:
    WaveformComponent();
    ~WaveformComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Set sample data to visualize
    void setSampleData(const std::vector<float>& data);
    
    // Clear waveform
    void clear();
    
private:
    std::vector<float> sampleData;
    
    // OP-1 style colors
    juce::Colour waveformColor;
    juce::Colour backgroundColor;
    juce::Colour gridColor;
    
    // Draw waveform using min/max method (OP-1 style)
    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds);
};

