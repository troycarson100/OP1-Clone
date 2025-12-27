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
    
    // Add orbit visualization (hidden by default)
    addChildComponent(&orbitVisualization);
    orbitVisualization.setVisible(false);
    orbitVisualization.setAlwaysOnTop(true);
    orbitVisualization.setInterceptsMouseClicks(false, false);
    
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
    
    // Reserve space for sample slots (under waveform)
    int sampleSlotHeight = 35;  // Less tall
    auto slotArea = bounds.removeFromBottom(sampleSlotHeight);
    waveformComponent.setBounds(bounds);
    
    // Sample slots positioned at bottom, spread across full width of screen component
    // Move up 10px from the calculated position
    sampleSlotComponent.setBounds(slotArea.getX(), slotArea.getY() - 10, slotArea.getWidth(), sampleSlotHeight);
    
    // Instrument menu fills entire screen component
    instrumentMenu.setBounds(getLocalBounds());
    
    // Orbit visualization fills entire screen component
    orbitVisualization.setBounds(getLocalBounds());
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
        // Hide waveform, sample slots, and orbit visualization when instrument menu is shown
        waveformComponent.setVisible(false);
        sampleSlotComponent.setVisible(false);
        orbitVisualization.setVisible(false);  // Ensure orbit menu is closed
        // Ensure menu covers entire screen component
        instrumentMenu.setBounds(getLocalBounds());
    } else {
        // Show waveform and sample slots when menu is hidden
        waveformComponent.setVisible(true);
        sampleSlotComponent.setVisible(true);
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
    // Also update orbit visualization if it's for slots A-D
    if (slotIndex >= 0 && slotIndex < 4) {
        orbitVisualization.setSlotPreview(slotIndex, sampleData);
        orbitVisualization.setSlotLoaded(slotIndex, !sampleData.empty());
    }
}

void ScreenComponent::showOrbitVisualization(bool show) {
    orbitVisualization.setVisible(show);
    if (show) {
        // Hide waveform, sample slots, and instrument menu when orbit visualization is shown
        waveformComponent.setVisible(false);
        sampleSlotComponent.setVisible(false);
        instrumentMenu.setVisible(false);  // Ensure instrument menu is closed
        orbitVisualization.setBounds(getLocalBounds());
    } else {
        // Show waveform and sample slots when orbit visualization is hidden
        waveformComponent.setVisible(true);
        sampleSlotComponent.setVisible(true);
    }
    repaint();
}

void ScreenComponent::setOrbitSlotPreview(int slotIndex, const std::vector<float>& sampleData) {
    orbitVisualization.setSlotPreview(slotIndex, sampleData);
    orbitVisualization.setSlotLoaded(slotIndex, !sampleData.empty());
}

void ScreenComponent::setOrbitWeights(const std::array<float, 4>& weights) {
    orbitVisualization.setOrbitWeights(weights);
}

void ScreenComponent::setOrbitShape(int shape) {
    orbitVisualization.setOrbitShape(shape);
}

void ScreenComponent::setOrbitRate(float rateHz) {
    orbitVisualization.setOrbitRate(rateHz);
}

void ScreenComponent::setOrbitPhase(float phase) {
    orbitVisualization.setOrbitPhase(phase);
}

