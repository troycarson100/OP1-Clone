#include "JuceEngineAdapter.h"
#include <algorithm>
#include <fstream>
#include <chrono>

JuceEngineAdapter::JuceEngineAdapter()
    : sourceSampleRate(44100.0)
{
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
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H3\",\"location\":\"JuceEngineAdapter.cpp:20\",\"message\":\"setSample entry\",\"data\":{\"numSamples\":" << buffer.getNumSamples() << ",\"sourceSampleRate\":" << sourceSampleRate << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    this->sourceSampleRate = sourceSampleRate;
    // Extract sample data from JUCE buffer
    // For now, use left channel only (mono)
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();
    
    // THREAD-SAFETY FIX: Extract data to temporary buffer first, then pass to engine
    // Engine will make its own copy, so we don't need to update adapter's vector yet
    std::vector<float> tempData(static_cast<size_t>(numSamples));
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"JuceEngineAdapter.cpp:38\",\"message\":\"temp buffer created\",\"data\":{\"numSamples\":" << numSamples << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    if (numChannels > 0 && numSamples > 0) {
        const float* channelData = buffer.getReadPointer(0);
        
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"JuceEngineAdapter.cpp:47\",\"message\":\"before copy to temp\",\"data\":{\"channelDataPtr\":" << (void*)channelData << ",\"numSamples\":" << numSamples << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
        
        if (channelData != nullptr) {
            std::copy(channelData, channelData + numSamples, tempData.begin());
        }
        
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"JuceEngineAdapter.cpp:57\",\"message\":\"after copy to temp\",\"data\":{},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
    } else {
        // Empty buffer - fill with zeros
        std::fill(tempData.begin(), tempData.end(), 0.0f);
    }
    
    // Create immutable SampleData object on heap (stable, thread-safe)
    // Allocate new SampleData, fill it completely, then atomically swap into engine
    // No pointers to local AudioBuffer memory are stored - everything is copied
    auto newSampleData = std::make_shared<Core::SampleData>();
    
    // Move tempData into SampleData (tempData is already a copy from AudioBuffer)
    newSampleData->mono = std::move(tempData);
    newSampleData->length = numSamples;
    newSampleData->sourceSampleRate = sourceSampleRate;
    
    // Validate before passing - ensure SampleData is fully constructed and valid
    if (newSampleData->mono.empty() || newSampleData->length <= 0 || 
        newSampleData->mono.data() == nullptr || 
        newSampleData->length != static_cast<int>(newSampleData->mono.size())) {
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H3\",\"location\":\"JuceEngineAdapter.cpp:87\",\"message\":\"Invalid SampleData - not setting\",\"data\":{\"empty\":" << (newSampleData->mono.empty() ? 1 : 0) << ",\"length\":" << newSampleData->length << ",\"size\":" << newSampleData->mono.size() << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
        return; // Don't set invalid sample
    }
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H3\",\"location\":\"JuceEngineAdapter.cpp:97\",\"message\":\"SampleData created and validated - atomically swapping\",\"data\":{\"length\":" << numSamples << ",\"size\":" << newSampleData->mono.size() << ",\"sampleRate\":" << sourceSampleRate << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // Atomically swap in new sample data (lock-free, thread-safe)
    // Uses atomic_store_explicit on plain shared_ptr (standard-safe)
    engine.setSampleData(newSampleData);
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"JuceEngineAdapter.cpp:92\",\"message\":\"after engine.setSampleData\",\"data\":{},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // Update adapter's vector for visualization (copy from SampleData)
    this->sampleData = newSampleData->mono;
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"JuceEngineAdapter.cpp:108\",\"message\":\"adapter vector updated\",\"data\":{\"newSize\":" << this->sampleData.size() << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"JuceEngineAdapter.cpp:87\",\"message\":\"after engine.setSample\",\"data\":{},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
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

void JuceEngineAdapter::setADSR(float attackMs, float decayMs, float sustain, float releaseMs) {
    engine.setADSR(attackMs, decayMs, sustain, releaseMs);
}

void JuceEngineAdapter::setRepitch(float semitones) {
    engine.setRepitch(semitones);
}

void JuceEngineAdapter::setStartPoint(int sampleIndex) {
    engine.setStartPoint(sampleIndex);
}

void JuceEngineAdapter::setEndPoint(int sampleIndex) {
    engine.setEndPoint(sampleIndex);
}

void JuceEngineAdapter::setSampleGain(float gain) {
    engine.setSampleGain(gain);
}

float JuceEngineAdapter::getRepitch() const {
    return engine.getRepitch();
}

int JuceEngineAdapter::getStartPoint() const {
    return engine.getStartPoint();
}

int JuceEngineAdapter::getEndPoint() const {
    return engine.getEndPoint();
}

float JuceEngineAdapter::getSampleGain() const {
    return engine.getSampleGain();
}


float JuceEngineAdapter::getGain() const {
    return 1.0f; // Core engine doesn't expose getter, return default
}

void JuceEngineAdapter::getSampleDataForVisualization(std::vector<float>& outData) const {
    // Thread-safe: copy sample data for visualization
    outData = sampleData;
}

double JuceEngineAdapter::getSourceSampleRate() const {
    return sourceSampleRate;
}

void JuceEngineAdapter::getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const {
    engine.getDebugInfo(actualInN, outN, primeRemaining, nonZeroCount);
}

double JuceEngineAdapter::getPlayheadPosition() const {
    return engine.getPlayheadPosition();
}

float JuceEngineAdapter::getEnvelopeValue() const {
    return engine.getEnvelopeValue();
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

void JuceEngineAdapter::setLPFilterCutoff(float cutoffHz) {
    engine.setLPFilterCutoff(cutoffHz);
}

void JuceEngineAdapter::setLPFilterResonance(float resonance) {
    engine.setLPFilterResonance(resonance);
}

void JuceEngineAdapter::setLPFilterEnvAmount(float amount) {
    engine.setLPFilterEnvAmount(amount);
}

void JuceEngineAdapter::setLPFilterDrive(float driveDb) {
    engine.setLPFilterDrive(driveDb);
}

// Time warp speed removed - fixed at 1.0 (constant duration)

void JuceEngineAdapter::setPlaybackMode(bool polyphonic) {
    engine.setPlaybackMode(polyphonic);
}

void JuceEngineAdapter::setLoopEnabled(bool enabled) {
    engine.setLoopEnabled(enabled);
}

void JuceEngineAdapter::setLoopPoints(int startPoint, int endPoint) {
    engine.setLoopPoints(startPoint, endPoint);
}

void JuceEngineAdapter::setWarpEnabled(bool enabled) {
    engine.setWarpEnabled(enabled);
}

void JuceEngineAdapter::setTimeRatio(double ratio) {
    engine.setTimeRatio(ratio);
}

void JuceEngineAdapter::setFilterEffectsEnabled(bool enabled) {
    engine.setFilterEffectsEnabled(enabled);
}

void JuceEngineAdapter::setLoopEnvAttack(float attackMs) {
    engine.setLoopEnvAttack(attackMs);
}

void JuceEngineAdapter::setLoopEnvRelease(float releaseMs) {
    engine.setLoopEnvRelease(releaseMs);
}

