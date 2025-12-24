#pragma once

#include "../Core/IVisualizationRenderer.h"
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * JUCE implementation of IVisualizationRenderer
 * This is the ONLY place where JUCE Graphics is used for visualization
 * All visualization logic goes through the abstract interface
 */
class JuceVisualizationRenderer : public Core::IVisualizationRenderer {
public:
    JuceVisualizationRenderer();
    ~JuceVisualizationRenderer() override;
    
    // Set the JUCE Graphics context for rendering
    // Must be called before each render operation
    void setGraphicsContext(juce::Graphics* g);
    
    // IVisualizationRenderer interface
    void renderWaveform(const Core::WaveformData& data, const Core::Rectangle& bounds) override;
    void renderADSR(const Core::ADSRData& data, const Core::Rectangle& bounds) override;
    void fillBackground(const Core::Color& color, const Core::Rectangle& bounds) override;

private:
    juce::Graphics* graphics;  // Current graphics context (set before rendering)
    
    // Helper: Convert Core::Color to juce::Colour
    juce::Colour toJuceColor(const Core::Color& color) const;
    
    // Helper: Convert Core::Rectangle to juce::Rectangle
    juce::Rectangle<int> toJuceRectangle(const Core::Rectangle& rect) const;
    
    // Internal waveform rendering helpers
    void drawWaveformPath(const Core::WaveformData& data, const Core::Rectangle& bounds);
    void drawWaveformPathForChannel(const std::vector<float>& channelData, const Core::WaveformData& data, const Core::Rectangle& bounds);
    void drawLoopMarkers(const Core::WaveformData& data, const Core::Rectangle& bounds);
    void drawLoopMarkersForChannel(const Core::WaveformData& data, const Core::Rectangle& bounds);
    void drawChannelLabel(const char* label, const Core::Rectangle& bounds);
    void drawPlayheads(const Core::WaveformData& data, const Core::Rectangle& bounds);  // Draw multiple playhead lines
    
    // Internal ADSR rendering helpers
    void drawADSREnvelope(const Core::ADSRData& data, const Core::Rectangle& bounds);
};

