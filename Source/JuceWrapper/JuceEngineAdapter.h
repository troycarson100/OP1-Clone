#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/SamplerEngine.h"
#include "../Core/MidiEvent.h"
#include <vector>

// Thin adapter layer between JUCE and portable Core engine
// Converts JUCE types to Core types
class JuceEngineAdapter {
public:
    JuceEngineAdapter();
    ~JuceEngineAdapter();
    
    // Prepare adapter (called from PluginProcessor::prepareToPlay)
    void prepare(double sampleRate, int blockSize, int numChannels);
    
    // Load sample from JUCE AudioBuffer (called after loading file)
    // Takes ownership of the buffer data
    void setSample(juce::AudioBuffer<float>& buffer, double sourceSampleRate);
    
    // Process audio block - converts JUCE buffer to Core format
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    
    // Set gain parameter
    void setGain(float gain);
    
    // Get current gain
    float getGain() const;
    
private:
    Core::SamplerEngine engine;
    
    // Pre-allocated buffers for conversion (no allocation in audio thread)
    std::vector<float*> channelPointers;
    std::vector<Core::MidiEvent> midiEventBuffer;
    
    // Sample data storage (owned by adapter)
    std::vector<float> sampleData;
    
    // Helper: convert JUCE MIDI buffer to Core MidiEvent array
    void convertMidiBuffer(juce::MidiBuffer& midiMessages, int numSamples);
};

