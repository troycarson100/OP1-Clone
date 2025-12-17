#include "JuceEngineAdapter.h"
#include <algorithm>

JuceEngineAdapter::JuceEngineAdapter() {
}

JuceEngineAdapter::~JuceEngineAdapter() {
}

void JuceEngineAdapter::prepare(double sampleRate, int blockSize, int numChannels) {
    engine.prepare(sampleRate, blockSize, numChannels);
    
    // Pre-allocate channel pointer array (max 8 channels should be enough)
    channelPointers.resize(std::max(numChannels, 8));
    midiEventBuffer.reserve(128); // Pre-allocate space for MIDI events
}

void JuceEngineAdapter::setSample(juce::AudioBuffer<float>& buffer, double sourceSampleRate) {
    // Extract sample data from JUCE buffer
    // For now, use left channel only (mono)
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();
    
    sampleData.resize(numSamples);
    
    if (numChannels > 0) {
        const float* channelData = buffer.getReadPointer(0);
        std::copy(channelData, channelData + numSamples, sampleData.begin());
    } else {
        // Empty buffer - fill with zeros
        std::fill(sampleData.begin(), sampleData.end(), 0.0f);
    }
    
    // Pass to core engine
    engine.setSample(sampleData.data(), static_cast<int>(sampleData.size()), sourceSampleRate);
}

void JuceEngineAdapter::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    
    // Convert JUCE buffer to float** format
    // Update channel pointers (these point to JUCE's internal buffers)
    for (int ch = 0; ch < numChannels; ++ch) {
        channelPointers[ch] = buffer.getWritePointer(ch);
    }
    
    // Convert MIDI messages
    convertMidiBuffer(midiMessages, numSamples);
    
    // Process through core engine
    engine.handleMidi(midiEventBuffer.data(), static_cast<int>(midiEventBuffer.size()));
    engine.process(channelPointers.data(), numChannels, numSamples);
    
    // Clear MIDI event buffer for next block
    midiEventBuffer.clear();
}

void JuceEngineAdapter::setGain(float gain) {
    engine.setGain(gain);
}

float JuceEngineAdapter::getGain() const {
    return 1.0f; // Core engine doesn't expose getter, return default
}

void JuceEngineAdapter::convertMidiBuffer(juce::MidiBuffer& midiMessages, int numSamples) {
    midiEventBuffer.clear();
    
    for (const auto metadata : midiMessages) {
        juce::MidiMessage message = metadata.getMessage();
        int sampleOffset = metadata.samplePosition;
        
        if (message.isNoteOn()) {
            int note = message.getNoteNumber();
            float velocity = message.getFloatVelocity();
            midiEventBuffer.emplace_back(Core::MidiEvent::NoteOn, note, velocity, sampleOffset);
        } else if (message.isNoteOff()) {
            int note = message.getNoteNumber();
            midiEventBuffer.emplace_back(Core::MidiEvent::NoteOff, note, 0.0f, sampleOffset);
        }
    }
}

