#include "JuceEngineAdapter.h"
#include <algorithm>
#include <fstream>
#include <chrono>
#include <array>
#include <cmath>

JuceEngineAdapter::JuceEngineAdapter()
    : sourceSampleRate(44100.0)
    , playbackMode(0)  // Default to Stacked
    , roundRobinIndex(0)
{
    // Initialize all slots as inactive
    for (int i = 0; i < 5; ++i) {
        activeSlots[i].store(false, std::memory_order_relaxed);
    }
}

JuceEngineAdapter::~JuceEngineAdapter() {
}

// Helper function to preprocess sample data for click reduction
// Simplified approach: basic DC removal and gentle fade-in only
void JuceEngineAdapter::preprocessSampleData(std::vector<float>& data) {
    if (data.empty()) return;
    
    int numSamples = static_cast<int>(data.size());
    
    // Step 1: Simple DC offset removal (average removal)
    float dcOffset = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        dcOffset += data[i];
    }
    dcOffset /= static_cast<float>(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        data[i] -= dcOffset;
    }
    
    // Step 2: Gentle fade-in at start (first 256 samples) to prevent clicks
    // Zero out first 2 samples, then fade in smoothly
    if (numSamples > 0) {
        data[0] = 0.0f;
    }
    if (numSamples > 1) {
        data[1] = 0.0f;
    }
    
    int fadeInSamples = std::min(256, numSamples - 2);
    for (int i = 2; i < 2 + fadeInSamples; ++i) {
        float t = static_cast<float>(i - 2) / static_cast<float>(fadeInSamples);
        // Use smooth sine curve for natural fade
        float fadeGain = std::sin(t * 1.5707963267948966f); // sin(PI/2 * t)
        data[i] *= fadeGain;
    }
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
    // Extract both left and right channels if stereo, otherwise use left channel only
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();
    
    // THREAD-SAFETY FIX: Extract data to temporary buffers first, then pass to engine
    // Engine will make its own copy, so we don't need to update adapter's vector yet
    std::vector<float> tempLeftData(static_cast<size_t>(numSamples));
    std::vector<float> tempRightData;  // Only allocate if stereo
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"JuceEngineAdapter.cpp:38\",\"message\":\"temp buffer created\",\"data\":{\"numSamples\":" << numSamples << ",\"numChannels\":" << numChannels << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    if (numChannels > 0 && numSamples > 0) {
        // Extract left channel (channel 0)
        const float* leftChannelData = buffer.getReadPointer(0);
        if (leftChannelData != nullptr) {
            std::copy(leftChannelData, leftChannelData + numSamples, tempLeftData.begin());
            // Apply comprehensive preprocessing for click reduction
            preprocessSampleData(tempLeftData);
        }
        
        // Extract right channel if stereo (channel 1)
        if (numChannels >= 2) {
            tempRightData.resize(static_cast<size_t>(numSamples));
            const float* rightChannelData = buffer.getReadPointer(1);
            if (rightChannelData != nullptr) {
                std::copy(rightChannelData, rightChannelData + numSamples, tempRightData.begin());
                // Apply comprehensive preprocessing for click reduction
                preprocessSampleData(tempRightData);
            }
        }
        
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"JuceEngineAdapter.cpp:57\",\"message\":\"after copy to temp\",\"data\":{\"isStereo\":" << (numChannels >= 2 ? 1 : 0) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
    } else {
        // Empty buffer - fill with zeros
        std::fill(tempLeftData.begin(), tempLeftData.end(), 0.0f);
    }
    
    // Create immutable SampleData object on heap (stable, thread-safe)
    // Allocate new SampleData, fill it completely, then atomically swap into engine
    // No pointers to local AudioBuffer memory are stored - everything is copied
    auto newSampleData = std::make_shared<Core::SampleData>();
    
    // Move tempData into SampleData (tempData is already a copy from AudioBuffer)
    newSampleData->mono = std::move(tempLeftData);
    newSampleData->right = std::move(tempRightData);
    newSampleData->length = numSamples;
    newSampleData->sourceSampleRate = sourceSampleRate;
    
    // Validate sample data before setting
    if (newSampleData->mono.empty() || newSampleData->length <= 0) {
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
    
    // Update adapter's vectors for visualization (copy from SampleData)
    this->sampleData = newSampleData->mono;
    this->rightChannelData = newSampleData->right;
    
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

void JuceEngineAdapter::setSampleForSlot(int slotIndex, juce::AudioBuffer<float>& buffer, double sourceSampleRate) {
    if (slotIndex < 0 || slotIndex >= 5) {
        return; // Invalid slot index
    }
    
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();
    
    // Extract sample data
    slotSamples[slotIndex].leftChannel.resize(static_cast<size_t>(numSamples));
    slotSamples[slotIndex].rightChannel.clear();
    
    if (numChannels > 0 && numSamples > 0) {
        const float* leftChannelData = buffer.getReadPointer(0);
        if (leftChannelData != nullptr) {
            std::copy(leftChannelData, leftChannelData + numSamples, slotSamples[slotIndex].leftChannel.begin());
            // Apply comprehensive preprocessing for click reduction
            preprocessSampleData(slotSamples[slotIndex].leftChannel);
        }
        
        if (numChannels >= 2) {
            slotSamples[slotIndex].rightChannel.resize(static_cast<size_t>(numSamples));
            const float* rightChannelData = buffer.getReadPointer(1);
            if (rightChannelData != nullptr) {
                std::copy(rightChannelData, rightChannelData + numSamples, slotSamples[slotIndex].rightChannel.begin());
                // Apply comprehensive preprocessing for click reduction
                preprocessSampleData(slotSamples[slotIndex].rightChannel);
            }
        }
    }
    
    slotSamples[slotIndex].sourceSampleRate = sourceSampleRate;
    slotSamples[slotIndex].hasSample = !slotSamples[slotIndex].leftChannel.empty();
}

void JuceEngineAdapter::setSlotRepitch(int slotIndex, float semitones) {
    if (slotIndex >= 0 && slotIndex < 5) {
        slotParameters[slotIndex].repitchSemitones = semitones;
    }
}

void JuceEngineAdapter::setSlotStartPoint(int slotIndex, int sampleIndex) {
    if (slotIndex >= 0 && slotIndex < 5) {
        slotParameters[slotIndex].startPoint = sampleIndex;
    }
}

void JuceEngineAdapter::setSlotEndPoint(int slotIndex, int sampleIndex) {
    if (slotIndex >= 0 && slotIndex < 5) {
        slotParameters[slotIndex].endPoint = sampleIndex;
    }
}

void JuceEngineAdapter::setSlotSampleGain(int slotIndex, float gain) {
    if (slotIndex >= 0 && slotIndex < 5) {
        slotParameters[slotIndex].sampleGain = gain;
    }
}

void JuceEngineAdapter::setSlotADSR(int slotIndex, float attackMs, float decayMs, float sustain, float releaseMs) {
    if (slotIndex >= 0 && slotIndex < 5) {
        slotParameters[slotIndex].attackMs = attackMs;
        slotParameters[slotIndex].decayMs = decayMs;
        slotParameters[slotIndex].sustain = sustain;
        slotParameters[slotIndex].releaseMs = releaseMs;
    }
}

void JuceEngineAdapter::setSlotLoopEnabled(int slotIndex, bool enabled) {
    if (slotIndex >= 0 && slotIndex < 5) {
        slotParameters[slotIndex].loopEnabled = enabled;
    }
}

void JuceEngineAdapter::setSlotLoopPoints(int slotIndex, int startPoint, int endPoint) {
    if (slotIndex >= 0 && slotIndex < 5) {
        slotParameters[slotIndex].loopStartPoint = startPoint;
        slotParameters[slotIndex].loopEndPoint = endPoint;
    }
}

float JuceEngineAdapter::getSlotRepitch(int slotIndex) const {
    if (slotIndex >= 0 && slotIndex < 5) {
        return slotParameters[slotIndex].repitchSemitones;
    }
    return 0.0f;
}

int JuceEngineAdapter::getSlotStartPoint(int slotIndex) const {
    if (slotIndex >= 0 && slotIndex < 5) {
        return slotParameters[slotIndex].startPoint;
    }
    return 0;
}

int JuceEngineAdapter::getSlotEndPoint(int slotIndex) const {
    if (slotIndex >= 0 && slotIndex < 5) {
        return slotParameters[slotIndex].endPoint;
    }
    return 0;
}

float JuceEngineAdapter::getSlotSampleGain(int slotIndex) const {
    if (slotIndex >= 0 && slotIndex < 5) {
        return slotParameters[slotIndex].sampleGain;
    }
    return 1.0f;
}

void JuceEngineAdapter::setPlaybackMode(int mode) {
    playbackMode = mode;
    roundRobinIndex = 0; // Reset round robin index when mode changes
}

void JuceEngineAdapter::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    
    // Convert JUCE buffer to float** format
    // Update channel pointers (these point to JUCE's internal buffers)
    for (int ch = 0; ch < numChannels; ++ch) {
        channelPointers[ch] = buffer.getWritePointer(ch);
    }
    
    // Convert MIDI messages and handle stacked/round robin playback
    convertMidiBuffer(midiMessages, numSamples);
    
        // Process MIDI events with stacked/round robin support
        // For stacked mode: trigger all loaded slots
        // For round robin: cycle through loaded slots
        std::vector<Core::MidiEvent> processedEvents;
        
        // Get list of loaded slots
        std::vector<int> loadedSlots;
        for (int i = 0; i < 5; ++i) {
            if (slotSamples[i].hasSample && !slotSamples[i].leftChannel.empty()) {
                loadedSlots.push_back(i);
            }
        }
    
    // If no slots loaded, fall back to default sample
    if (loadedSlots.empty()) {
        // Use default engine sample
        engine.handleMidi(midiEventBuffer.data(), static_cast<int>(midiEventBuffer.size()));
    } else if (playbackMode == 0) {
        // Stacked mode: trigger all loaded slots (even if just one)
        // Stacked mode with multiple slots: trigger all loaded slots
        // Process each MIDI event
        for (const auto& event : midiEventBuffer) {
            if (event.type == Core::MidiEvent::NoteOn) {
                // Process each slot separately so each gets its own sample
                // Use different MIDI notes for each slot to ensure separate voices
                int baseNote = event.note;
                int slotOffset = 0;
                for (int slotIndex : loadedSlots) {
                    // Get this slot's parameters
                    const SlotParameters& params = slotParameters[slotIndex];
                    
                    // Create SampleData for this slot
                    auto slotSampleData = std::make_shared<Core::SampleData>();
                    slotSampleData->mono = slotSamples[slotIndex].leftChannel;
                    slotSampleData->right = slotSamples[slotIndex].rightChannel;
                    slotSampleData->length = static_cast<int>(slotSamples[slotIndex].leftChannel.size());
                    slotSampleData->sourceSampleRate = slotSamples[slotIndex].sourceSampleRate;
                    
                    // Mark this slot as active
                    activeSlots[slotIndex].store(true, std::memory_order_relaxed);
                    
                    // Trigger note on with this slot's sample data and parameters
                    // Parameters are applied directly to the allocated voice, not globally
                    // This allows each slot to have independent parameters
                    // Use note + slotOffset to create unique notes (wrapped to stay in valid range)
                    int uniqueNote = (baseNote + slotOffset) % 128;  // Wrap to valid MIDI range
                    engine.triggerNoteOnWithSample(uniqueNote, event.velocity, slotSampleData,
                                                   params.repitchSemitones, params.startPoint, params.endPoint, params.sampleGain,
                                                   params.attackMs, params.decayMs, params.sustain, params.releaseMs,
                                                   params.loopEnabled, params.loopStartPoint, params.loopEndPoint);
                    slotOffset++;
                }
            } else if (event.type == Core::MidiEvent::NoteOff) {
                // NoteOff: clear active slots for all loaded slots that were playing this note
                // In stacked mode, all slots play together, so clear all when note is released
                for (int slotIndex : loadedSlots) {
                    activeSlots[slotIndex].store(false, std::memory_order_relaxed);
                }
                
                // NoteOff: send to all voices that might be playing this note (from any slot)
                // Since we used different note numbers for each slot, we need to send NoteOff for all of them
                int baseNote = event.note;
                for (size_t i = 0; i < loadedSlots.size(); ++i) {
                    Core::MidiEvent noteOffEvent = event;
                    noteOffEvent.note = (baseNote + static_cast<int>(i)) % 128;
                    processedEvents.push_back(noteOffEvent);
                }
            } else {
                // Other events - process normally
                processedEvents.push_back(event);
            }
        }
        
        // Process NoteOff and other events
        if (!processedEvents.empty()) {
            engine.handleMidi(processedEvents.data(), static_cast<int>(processedEvents.size()));
        }
    } else {
        // Round robin mode: cycle through loaded slots
        for (const auto& event : midiEventBuffer) {
            if (event.type == Core::MidiEvent::NoteOn) {
                // Round robin mode: cycle through loaded slots
                int slotIndex = loadedSlots[roundRobinIndex % loadedSlots.size()];
                roundRobinIndex = (roundRobinIndex + 1) % loadedSlots.size();
                
                // Get this slot's parameters
                const SlotParameters& params = slotParameters[slotIndex];
                
                // Create SampleData for this slot
                auto slotSampleData = std::make_shared<Core::SampleData>();
                slotSampleData->mono = slotSamples[slotIndex].leftChannel;
                slotSampleData->right = slotSamples[slotIndex].rightChannel;
                slotSampleData->length = static_cast<int>(slotSamples[slotIndex].leftChannel.size());
                slotSampleData->sourceSampleRate = slotSamples[slotIndex].sourceSampleRate;
                
                // Mark this slot as active
                activeSlots[slotIndex].store(true, std::memory_order_relaxed);
                
                // Trigger note with this slot's sample data and parameters
                engine.triggerNoteOnWithSample(event.note, event.velocity, slotSampleData,
                                               params.repitchSemitones, params.startPoint, params.endPoint, params.sampleGain,
                                               params.attackMs, params.decayMs, params.sustain, params.releaseMs,
                                               params.loopEnabled, params.loopStartPoint, params.loopEndPoint);
            } else if (event.type == Core::MidiEvent::NoteOff) {
                // NoteOff: clear active slot for the slot that was playing
                // In round robin mode, only one slot plays at a time
                int slotIndex = loadedSlots[(roundRobinIndex - 1 + loadedSlots.size()) % loadedSlots.size()];
                activeSlots[slotIndex].store(false, std::memory_order_relaxed);
                
                // NoteOff events pass through unchanged
                processedEvents.push_back(event);
            } else {
                // Other events pass through unchanged
                processedEvents.push_back(event);
            }
        }
        
        // Process NoteOff and other events
        if (!processedEvents.empty()) {
            engine.handleMidi(processedEvents.data(), static_cast<int>(processedEvents.size()));
        }
    }
    
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
    // Thread-safe: copy sample data for visualization (mono/left channel only for backward compatibility)
    outData = sampleData;
}

void JuceEngineAdapter::getStereoSampleDataForVisualization(std::vector<float>& outLeft, std::vector<float>& outRight) const {
    // Thread-safe: copy stereo sample data for visualization
    outLeft = sampleData;
    outRight = rightChannelData;
}

void JuceEngineAdapter::getSlotStereoSampleDataForVisualization(int slotIndex, std::vector<float>& outLeft, std::vector<float>& outRight) const {
    // Thread-safe: copy stereo sample data for a specific slot
    if (slotIndex >= 0 && slotIndex < 5) {
        outLeft = slotSamples[slotIndex].leftChannel;
        outRight = slotSamples[slotIndex].rightChannel;
    } else {
        outLeft.clear();
        outRight.clear();
    }
}

double JuceEngineAdapter::getSourceSampleRate() const {
    return sourceSampleRate;
}

double JuceEngineAdapter::getSlotSourceSampleRate(int slotIndex) const {
    if (slotIndex >= 0 && slotIndex < 5) {
        return slotSamples[slotIndex].sourceSampleRate;
    }
    return 44100.0;  // Default fallback
}

std::array<bool, 5> JuceEngineAdapter::getActiveSlots() const {
    std::array<bool, 5> result;
    for (int i = 0; i < 5; ++i) {
        result[i] = activeSlots[i].load(std::memory_order_relaxed);
    }
    return result;
}

int JuceEngineAdapter::getActiveVoiceCount() const {
    return engine.getActiveVoicesCount();
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

void JuceEngineAdapter::getAllActivePlayheads(std::vector<double>& positions, std::vector<float>& envelopeValues) const {
    engine.getAllActivePlayheads(positions, envelopeValues);
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

void JuceEngineAdapter::convertMidiBuffer(juce::MidiBuffer& midiMessages, int numSamples) {
    midiEventBuffer.clear();
    
    for (const auto metadata : midiMessages) {
        const juce::MidiMessage& message = metadata.getMessage();
        
        Core::MidiEvent event;
        event.sampleOffset = metadata.samplePosition;
        
        if (message.isNoteOn()) {
            event.type = Core::MidiEvent::NoteOn;
            event.note = message.getNoteNumber();
            event.velocity = message.getFloatVelocity();
        } else if (message.isNoteOff()) {
            event.type = Core::MidiEvent::NoteOff;
            event.note = message.getNoteNumber();
            event.velocity = 0.0f;
        } else {
            // Ignore other MIDI message types for now
            continue;
        }
        
        midiEventBuffer.push_back(event);
    }
}
