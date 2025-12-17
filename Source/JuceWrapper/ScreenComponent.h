#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "WaveformComponent.h"

// Main screen component for OP-1 Clone
// Contains all visuals and menus for the synth
class ScreenComponent : public juce::Component {
public:
    ScreenComponent();
    ~ScreenComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Set sample data for waveform visualization
    void setSampleData(const std::vector<float>& data);
    
private:
    WaveformComponent waveformComponent;
    
    // Screen background and styling
    juce::Colour backgroundColor;
    juce::Colour borderColor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenComponent)
};

