#include "ScreenComponent.h"

ScreenComponent::ScreenComponent() {
    // OP-1 style colors
    backgroundColor = juce::Colour(0xFF1A1A1A); // Dark gray background
    borderColor = juce::Colour(0xFF333333);     // Lighter gray border
    
    // Add waveform component
    addAndMakeVisible(&waveformComponent);
    
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
    waveformComponent.setBounds(bounds);
    
    // Instrument menu fills entire screen component
    instrumentMenu.setBounds(getLocalBounds());
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

