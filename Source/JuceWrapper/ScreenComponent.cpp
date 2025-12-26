#include "ScreenComponent.h"

ScreenComponent::ScreenComponent()
    : selectedSlot(0)  // Start with slot A selected
{
    // OP-1 style colors
    backgroundColor = juce::Colours::black; // Black background
    borderColor = juce::Colour(0xFF333333);     // Lighter gray border
    
    // Add waveform component
    addAndMakeVisible(&waveformComponent);
    
    // Add sample slot component (5 slots A-E)
    sampleSlotComponent.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(&sampleSlotComponent);
    
    // Add instrument menu (hidden by default)
    // Menu should be on top and opaque to cover everything
    addChildComponent(&instrumentMenu);
    instrumentMenu.setVisible(false);
    instrumentMenu.setAlwaysOnTop(true);
    
    setOpaque(true);
}

ScreenComponent::~ScreenComponent() {
}

void ScreenComponent::paint(juce::Graphics& g) {
    // Fill background
    g.fillAll(backgroundColor);
    
    // Draw border
    g.setColour(borderColor);
    g.drawRect(getLocalBounds(), 2);
}

void ScreenComponent::resized() {
    // Waveform takes up most of the screen, but leave space at bottom for parameter displays
    auto bounds = getLocalBounds().reduced(5);
    // Reserve space at bottom for parameter displays (2 rows of 35px + spacing = ~80px)
    int paramDisplayAreaHeight = 80;
    bounds.removeFromBottom(paramDisplayAreaHeight);
    
    // Reserve space for sample slots (under waveform, left of ADSR pill)
    int sampleSlotHeight = 35;  // Less tall
    auto slotArea = bounds.removeFromBottom(sampleSlotHeight);
    waveformComponent.setBounds(bounds);
    
    // Sample slots positioned at bottom left (before ADSR pill area)
    // Leave space on right for ADSR pill (about 30% of width)
    // Move up 10px from the calculated position
    int slotWidth = static_cast<int>(slotArea.getWidth() * 0.75f);  // Wider (75% instead of 70%)
    sampleSlotComponent.setBounds(slotArea.getX(), slotArea.getY() - 10, slotWidth, sampleSlotHeight);
    
    // Instrument menu fills entire screen component
    instrumentMenu.setBounds(getLocalBounds());
}

void ScreenComponent::setSelectedSlot(int slotIndex) {
    if (slotIndex >= 0 && slotIndex < 5) {
        selectedSlot = slotIndex;
        sampleSlotComponent.setSelectedSlot(slotIndex);
        repaint();
    }
}

void ScreenComponent::setActiveSlots(const std::array<bool, 5>& activeSlots) {
    sampleSlotComponent.setActiveSlots(activeSlots);
}

void ScreenComponent::setSampleData(const std::vector<float>& data) {
    waveformComponent.setSampleData(data);
}

void ScreenComponent::setStereoSampleData(const std::vector<float>& leftChannel, const std::vector<float>& rightChannel) {
    waveformComponent.setStereoSampleData(leftChannel, rightChannel);
}

void ScreenComponent::setStartPoint(int sampleIndex) {
    waveformComponent.setStartPoint(sampleIndex);
}

void ScreenComponent::setEndPoint(int sampleIndex) {
    waveformComponent.setEndPoint(sampleIndex);
}

void ScreenComponent::setSampleGain(float gain) {
    waveformComponent.setSampleGain(gain);
}

void ScreenComponent::setLoopStartPoint(int sampleIndex) {
    waveformComponent.setLoopStartPoint(sampleIndex);
}

void ScreenComponent::setLoopEndPoint(int sampleIndex) {
    waveformComponent.setLoopEndPoint(sampleIndex);
}

void ScreenComponent::setLoopEnabled(bool enabled) {
    waveformComponent.setLoopEnabled(enabled);
}

void ScreenComponent::setPlayheadPosition(double sampleIndex, float envelopeValue) {
    // DEPRECATED - use setPlayheadPositions for multi-voice support
    std::vector<double> positions;
    std::vector<float> envelopeVals;
    if (sampleIndex >= 0.0 && envelopeValue > 0.0f) {
        positions.push_back(sampleIndex);
        envelopeVals.push_back(envelopeValue);
    }
    waveformComponent.setPlayheadPositions(positions, envelopeVals);
}

void ScreenComponent::setPlayheadPositions(const std::vector<double>& positions, const std::vector<float>& envelopeValues) {
    waveformComponent.setPlayheadPositions(positions, envelopeValues);
}

void ScreenComponent::showInstrumentMenu(bool show) {
    instrumentMenu.setMenuVisible(show);
    if (show) {
        instrumentMenu.setSelectedIndex(0);  // Reset to first item
        // Hide waveform component when menu is shown
        waveformComponent.setVisible(false);
        // Ensure menu covers entire screen component
        instrumentMenu.setBounds(getLocalBounds());
    } else {
        // Show waveform component when menu is hidden
        waveformComponent.setVisible(true);
    }
    repaint();
}

void ScreenComponent::setInstrumentMenuSelectedIndex(int index) {
    instrumentMenu.setSelectedIndex(index);
}

void ScreenComponent::selectInstrument() {
    juce::String selected = instrumentMenu.getSelectedInstrument();
    if (!selected.isEmpty()) {
        // Trigger callback if set
        if (instrumentMenu.onInstrumentSelected) {
            instrumentMenu.onInstrumentSelected(selected);
        }
        instrumentMenu.setMenuVisible(false);
        repaint();
    }
}

void ScreenComponent::setInstrumentMenuCallback(std::function<void(const juce::String&)> callback) {
    instrumentMenu.onInstrumentSelected = callback;
}

void ScreenComponent::setSlotAPreview(const std::vector<float>& sampleData) {
    // Legacy method - delegate to new method
    setSlotPreview(0, sampleData);
}

void ScreenComponent::setSlotPreview(int slotIndex, const std::vector<float>& sampleData) {
    sampleSlotComponent.setSlotPreview(slotIndex, sampleData);
}

