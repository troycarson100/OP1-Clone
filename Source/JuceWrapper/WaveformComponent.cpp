#include "WaveformComponent.h"
#include <algorithm>

WaveformComponent::WaveformComponent()
    : renderer(std::make_unique<JuceVisualizationRenderer>())
    , startPoint(0)
    , endPoint(0)
    , sampleGain(1.0f)
    , loopStartPoint(0)
    , loopEndPoint(0)
    , loopEnabled(false)
{
    setOpaque(true);
}

WaveformComponent::~WaveformComponent()
{
}

void WaveformComponent::setSampleData(const std::vector<float>& data)
{
    // Mono data - store in left channel only
    leftChannelData = data;
    rightChannelData.clear();
    // Initialize end point to full sample length
    if (endPoint == 0 && !data.empty()) {
        endPoint = static_cast<int>(data.size());
    }
    repaint();
}

void WaveformComponent::setStereoSampleData(const std::vector<float>& leftChannel, const std::vector<float>& rightChannel)
{
    leftChannelData = leftChannel;
    rightChannelData = rightChannel;
    // Initialize end point to full sample length
    if (endPoint == 0 && !leftChannel.empty()) {
        endPoint = static_cast<int>(leftChannel.size());
    }
    repaint();
}

void WaveformComponent::setStartPoint(int sampleIndex)
{
    startPoint = sampleIndex;
    repaint();
}

void WaveformComponent::setEndPoint(int sampleIndex)
{
    endPoint = sampleIndex;
    repaint();
}

void WaveformComponent::setSampleGain(float gain)
{
    sampleGain = gain;
    repaint();
}

void WaveformComponent::clear()
{
    leftChannelData.clear();
    rightChannelData.clear();
    repaint();
}

void WaveformComponent::updateSmoothedPlayheads(const std::vector<double>& positions, const std::vector<float>& envelopeValues)
{
    // Resize smoothed playheads vector if needed
    if (smoothedPlayheads.size() < positions.size()) {
        smoothedPlayheads.resize(positions.size());
    }
    
    // Update existing playheads and mark inactive ones
    for (size_t i = 0; i < smoothedPlayheads.size(); ++i) {
        if (i < positions.size() && envelopeValues[i] > 0.0f) {
            // Active playhead - update target and smooth
            smoothedPlayheads[i].targetPos = positions[i];
            smoothedPlayheads[i].envelopeValue = envelopeValues[i];
            smoothedPlayheads[i].active = true;
            
            // Smooth towards target position
            if (smoothedPlayheads[i].currentPos < 0.0) {
                // First time - initialize to start point (not current position)
                // This ensures playheads always start from the beginning of the sample
                smoothedPlayheads[i].currentPos = static_cast<double>(startPoint);
            } else {
                // Smooth interpolation
                double diff = smoothedPlayheads[i].targetPos - smoothedPlayheads[i].currentPos;
                smoothedPlayheads[i].currentPos += diff * smoothingCoeff;
            }
        } else {
            // Inactive - fade out smoothly
            smoothedPlayheads[i].active = false;
            smoothedPlayheads[i].envelopeValue = 0.0f;
            // Keep currentPos for smooth fade-out, but mark as inactive
        }
    }
}

Core::WaveformData WaveformComponent::buildWaveformData() const
{
    Core::WaveformData data;
    
    // Copy sample data (stereo if available, otherwise mono)
    data.leftChannel = leftChannelData;
    data.rightChannel = rightChannelData;
    
    // Copy all parameters
    data.startPoint = startPoint;
    data.endPoint = endPoint;
    data.sampleGain = sampleGain;
    data.loopStartPoint = loopStartPoint;
    data.loopEndPoint = loopEndPoint;
    data.loopEnabled = loopEnabled;
    
    // Copy smoothed playhead positions
    data.playheads.clear();
    for (const auto& smoothed : smoothedPlayheads) {
        if (smoothed.active && smoothed.envelopeValue > 0.0f && smoothed.currentPos >= 0.0) {
            Core::WaveformData::PlayheadInfo info;
            info.position = smoothed.currentPos;
            info.envelopeValue = smoothed.envelopeValue;
            data.playheads.push_back(info);
        }
    }
    
    // Set colors (OP-1 style)
    data.waveformColor = Core::Color(255, 255, 255);      // White
    data.backgroundColor = Core::Color(10, 10, 10);        // Very dark
    data.gridColor = Core::Color(34, 34, 34);             // Subtle grid
    data.markerColor = Core::Color(255, 255, 255);        // White
    data.playheadColor = Core::Color(255, 255, 0, 120);    // Yellow, ~47% opacity (120/255) - fainter
    
    return data;
}

void WaveformComponent::paint(juce::Graphics& g)
{
    // Set graphics context for renderer
    renderer->setGraphicsContext(&g);
    
    // Build waveform data from current state
    Core::WaveformData waveformData = buildWaveformData();
    
    // Get bounds (reduced by 2px padding for waveform content)
    auto bounds = getLocalBounds().reduced(2);
    Core::Rectangle renderBounds(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight());
    
    // Fill background first (full component bounds)
    auto fullBounds = getLocalBounds();
    Core::Rectangle bgBounds(fullBounds.getX(), fullBounds.getY(), fullBounds.getWidth(), fullBounds.getHeight());
    renderer->fillBackground(waveformData.backgroundColor, bgBounds);
    
    // Render waveform in content area
    renderer->renderWaveform(waveformData, renderBounds);
}

void WaveformComponent::resized()
{
    // Component will repaint automatically
}

void WaveformComponent::setLoopStartPoint(int sampleIndex)
{
    loopStartPoint = sampleIndex;
    repaint();
}

void WaveformComponent::setLoopEndPoint(int sampleIndex)
{
    loopEndPoint = sampleIndex;
    repaint();
}

void WaveformComponent::setLoopEnabled(bool enabled)
{
    loopEnabled = enabled;
    repaint();
}

void WaveformComponent::setPlayheadPosition(double sampleIndex, float envValue)
{
    // DEPRECATED - use setPlayheadPositions for multi-voice support
    std::vector<double> positions;
    std::vector<float> envelopeVals;
    if (sampleIndex >= 0.0 && envValue > 0.0f) {
        positions.push_back(sampleIndex);
        envelopeVals.push_back(envValue);
    }
    setPlayheadPositions(positions, envelopeVals);
}

void WaveformComponent::setPlayheadPositions(const std::vector<double>& positions, const std::vector<float>& envelopeValues)
{
    updateSmoothedPlayheads(positions, envelopeValues);
    repaint();
}
