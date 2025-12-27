#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "WaveformComponent.h"
#include "InstrumentMenuComponent.h"
#include "SampleSlotComponent.h"
#include "OrbitVisualizationComponent.h"

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
    
    // Set stereo sample data for waveform visualization
    void setStereoSampleData(const std::vector<float>& leftChannel, const std::vector<float>& rightChannel);
    
    // Set start/end points and sample gain for visualization
    void setStartPoint(int sampleIndex);
    void setEndPoint(int sampleIndex);
    void setSampleGain(float gain);
    
    // Set loop points and enable state
    void setLoopStartPoint(int sampleIndex);
    void setLoopEndPoint(int sampleIndex);
    void setLoopEnabled(bool enabled);
    
    void setPlayheadPosition(double sampleIndex, float envelopeValue);  // DEPRECATED - use setPlayheadPositions
    void setPlayheadPositions(const std::vector<double>& positions, const std::vector<float>& envelopeValues);
    
    // Get waveform component bounds (for ADSR overlay positioning)
    juce::Rectangle<int> getWaveformBounds() const { return waveformComponent.getBounds(); }
    
    // Set sample preview for slot A
    void setSlotAPreview(const std::vector<float>& sampleData);  // DEPRECATED
    void setSlotPreview(int slotIndex, const std::vector<float>& sampleData);
    
    // Slot selection control
    void setSelectedSlot(int slotIndex);  // 0-4 for A-E
    int getSelectedSlot() const { return selectedSlot; }
    
    // Set active slots (slots that are currently playing)
    void setActiveSlots(const std::array<bool, 5>& activeSlots);
    
    // Instrument menu control
    void showInstrumentMenu(bool show);
    bool isInstrumentMenuVisible() const { return instrumentMenu.isVisible(); }  // Uses Component::isVisible()
    void setInstrumentMenuSelectedIndex(int index);
    int getInstrumentMenuSelectedIndex() const { return instrumentMenu.getSelectedIndex(); }
    void selectInstrument();  // Selects the currently highlighted instrument
    void setInstrumentMenuCallback(std::function<void(const juce::String&)> callback);
    
    // Orbit visualization control
    void showOrbitVisualization(bool show);
    bool isOrbitVisualizationVisible() const { return orbitVisualization.isVisible(); }
    void setOrbitSlotPreview(int slotIndex, const std::vector<float>& sampleData);
    void setOrbitWeights(const std::array<float, 4>& weights);
    void setOrbitShape(int shape);
    void setOrbitRate(float rateHz);
    void setOrbitPhase(float phase);
    
private:
    WaveformComponent waveformComponent;
    InstrumentMenuComponent instrumentMenu;
    SampleSlotComponent sampleSlotComponent;
    OrbitVisualizationComponent orbitVisualization;
    
    int selectedSlot;  // Currently selected slot (0-4 for A-E)
    
    // Screen background and styling
    juce::Colour backgroundColor;
    juce::Colour borderColor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenComponent)
};

