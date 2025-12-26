#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>

// Component for displaying 5 sample slots (A, B, C, D, E)
// Similar to Pigments sampler - allows stacking or round robin
class SampleSlotComponent : public juce::Component {
public:
    SampleSlotComponent();
    ~SampleSlotComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Set sample preview data for slot A (first slot) - DEPRECATED, use setSlotPreview
    void setSlotAPreview(const std::vector<float>& sampleData);
    
    // Set sample preview data for a specific slot (0-4 for A-E)
    void setSlotPreview(int slotIndex, const std::vector<float>& sampleData);
    
    // Set selected slot (0-4 for A-E)
    void setSelectedSlot(int slotIndex);

private:
    std::array<std::vector<float>, 5> slotPreviewData;  // Preview data for each slot
    static constexpr int NUM_SLOTS = 5;
    int selectedSlotIndex;  // Currently selected slot (0-4)
    
    void drawSlot(juce::Graphics& g, int slotIndex, const juce::Rectangle<int>& bounds);
    void drawWaveformPreview(juce::Graphics& g, const juce::Rectangle<int>& bounds, const std::vector<float>& data);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleSlotComponent)
};

