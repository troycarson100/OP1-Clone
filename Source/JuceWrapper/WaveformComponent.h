#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "JuceVisualizationRenderer.h"
#include <vector>
#include <memory>

// OP-1 style waveform visualizer
// Uses IVisualizationRenderer interface - no direct JUCE Graphics usage
class WaveformComponent : public juce::Component {
public:
    WaveformComponent();
    ~WaveformComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Set sample data to visualize (mono - will be stored in leftChannel)
    void setSampleData(const std::vector<float>& data);
    
    // Set stereo sample data to visualize (left and right channels)
    void setStereoSampleData(const std::vector<float>& leftChannel, const std::vector<float>& rightChannel);
    
    // Set start/end points for visual markers
    void setStartPoint(int sampleIndex);
    void setEndPoint(int sampleIndex);
    
    // Set sample gain for visual scaling
    void setSampleGain(float gain);
    
    // Set loop points and enable state
    void setLoopStartPoint(int sampleIndex);
    void setLoopEndPoint(int sampleIndex);
    void setLoopEnabled(bool enabled);
    
    // Set playhead position (for yellow line indicator) - DEPRECATED, use setPlayheadPositions
    void setPlayheadPosition(double sampleIndex, float envelopeValue);
    
    // Set playhead positions (one per active voice)
    void setPlayheadPositions(const std::vector<double>& positions, const std::vector<float>& envelopeValues);
    
    // Clear waveform
    void clear();

private:
    // Renderer instance (JUCE implementation)
    std::unique_ptr<JuceVisualizationRenderer> renderer;
    
    // Sample data (stereo: left and right channels)
    std::vector<float> leftChannelData;
    std::vector<float> rightChannelData;
    
    // Sample editing parameters
    int startPoint;
    int endPoint;
    float sampleGain;
    
    // Loop parameters
    int loopStartPoint;
    int loopEndPoint;
    bool loopEnabled;
    
    // Playhead positions (one per active voice) with smoothing
    struct SmoothedPlayhead {
        double currentPos;      // Current smoothed position
        double targetPos;        // Target position to smooth towards
        float envelopeValue;     // Envelope value for this voice
        bool active;             // Whether this playhead is active
        
        SmoothedPlayhead() : currentPos(-1.0), targetPos(-1.0), envelopeValue(0.0f), active(false) {}
    };
    std::vector<SmoothedPlayhead> smoothedPlayheads;
    
    // Smoothing coefficient (0.0 = no smoothing, 1.0 = full smoothing)
    static constexpr float smoothingCoeff = 0.08f;  // Very smooth movement (8% towards target per frame)
    
    // Build Core::WaveformData from current state
    Core::WaveformData buildWaveformData() const;
    
    // Update smoothed playhead positions
    void updateSmoothedPlayheads(const std::vector<double>& positions, const std::vector<float>& envelopeValues);
};
