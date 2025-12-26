#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <cmath>
#include <fstream>
#include <chrono>

// For now, we'll generate a simple test tone as the default sample
// In a real implementation, you'd load a WAV file from BinaryData
static void generateTestTone(juce::AudioBuffer<float>& buffer, double sampleRate, float frequency, float duration) {
    int numSamples = static_cast<int>(sampleRate * duration);
    // Generate stereo test tone (2 channels)
    buffer.setSize(2, numSamples, false, true, false);
    
    float* leftChannelData = buffer.getWritePointer(0);
    float* rightChannelData = buffer.getWritePointer(1);
    float phase = 0.0f;
    float phaseIncrement = static_cast<float>(2.0 * juce::MathConstants<double>::pi * frequency / sampleRate);
    
    for (int i = 0; i < numSamples; ++i) {
        float sample = std::sin(phase) * 0.5f;
        leftChannelData[i] = sample;
        rightChannelData[i] = sample;  // Same signal in both channels for stereo
        phase += phaseIncrement;
        if (phase > 2.0f * juce::MathConstants<float>::pi) {
            phase -= 2.0f * juce::MathConstants<float>::pi;
        }
    }
}

Op1CloneAudioProcessor::Op1CloneAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
    )
    , parameters(*this, nullptr, juce::Identifier("Op1Clone"),
        {
            std::make_unique<juce::AudioParameterFloat>("gain", "Gain", 
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f)
        })
    , midiFifo(32)
{
}

Op1CloneAudioProcessor::~Op1CloneAudioProcessor() {
}

const juce::String Op1CloneAudioProcessor::getName() const {
    return "Op1Clone";
}

bool Op1CloneAudioProcessor::acceptsMidi() const {
    return true;
}

bool Op1CloneAudioProcessor::producesMidi() const {
    return false;
}

bool Op1CloneAudioProcessor::isMidiEffect() const {
    return false;
}

double Op1CloneAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int Op1CloneAudioProcessor::getNumPrograms() {
    return 1;
}

int Op1CloneAudioProcessor::getCurrentProgram() {
    return 0;
}

void Op1CloneAudioProcessor::setCurrentProgram(int index) {
}

const juce::String Op1CloneAudioProcessor::getProgramName(int index) {
    return {};
}

void Op1CloneAudioProcessor::changeProgramName(int index, const juce::String& newName) {
}

void Op1CloneAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    adapter.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    
    // Load default sample on first prepare
    static bool sampleLoaded = false;
    if (!sampleLoaded) {
        loadDefaultSample();
        sampleLoaded = true;
    }
}

void Op1CloneAudioProcessor::releaseResources() {
}

bool Op1CloneAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    
    return true;
}

void Op1CloneAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    
    // Drain MIDI input FIFO (from MIDI controller) - lock-free
    midiInputHandler.drainToMidiBuffer(midiMessages, buffer.getNumSamples());
    
    // Inject any pending MIDI messages from test button (lock-free)
    int count = pendingMidiCount.load(std::memory_order_acquire);
    if (count > 0) {
        for (int i = 0; i < count && i < 32; ++i) {
            if (midiMessageBuffer[i].getRawDataSize() > 0) {
                midiMessages.addEvent(midiMessageBuffer[i], 0);
            }
        }
        pendingMidiCount.store(0, std::memory_order_release);
    }
    
    // Update gain parameter
    float gainValue = *parameters.getRawParameterValue("gain");
    adapter.setGain(gainValue);
    
    // Update time-warp enable from atomic flag
    
    // Process through adapter
    adapter.processBlock(buffer, midiMessages);
    
    // Update debug info (read from adapter, safe atomic write)
    int actualInN, outN, primeRemaining, nonZeroCount;
    adapter.getDebugInfo(actualInN, outN, primeRemaining, nonZeroCount);
    debugLastActualInN.store(actualInN, std::memory_order_relaxed);
    debugLastOutN.store(outN, std::memory_order_relaxed);
    debugLastPrimeRemaining.store(primeRemaining, std::memory_order_relaxed);
    debugLastNonZeroOutCount.store(nonZeroCount, std::memory_order_relaxed);
}

bool Op1CloneAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* Op1CloneAudioProcessor::createEditor() {
    return new Op1CloneAudioProcessorEditor(*this);
}

void Op1CloneAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // Save state (for now, just save parameters)
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Op1CloneAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    // Load state
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName(parameters.state.getType())) {
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

void Op1CloneAudioProcessor::loadDefaultSample() {
    // Generate a simple 440Hz sine wave as test sample (1 second)
    juce::AudioBuffer<float> testBuffer;
    generateTestTone(testBuffer, 44100.0, 440.0f, 1.0);
    
    // Pass to adapter (sets default sample for backward compatibility)
    adapter.setSample(testBuffer, 44100.0);
    
    // Also save to slot 0 (default slot)
    adapter.setSampleForSlot(0, testBuffer, 44100.0);
}


void Op1CloneAudioProcessor::setADSR(float attackMs, float decayMs, float sustain, float releaseMs) {
    adapter.setADSR(attackMs, decayMs, sustain, releaseMs);
}

void Op1CloneAudioProcessor::setRepitch(float semitones) {
    adapter.setRepitch(semitones);
}

void Op1CloneAudioProcessor::setStartPoint(int sampleIndex) {
    adapter.setStartPoint(sampleIndex);
}

void Op1CloneAudioProcessor::setEndPoint(int sampleIndex) {
    adapter.setEndPoint(sampleIndex);
}

void Op1CloneAudioProcessor::setSampleGain(float gain) {
    adapter.setSampleGain(gain);
}

float Op1CloneAudioProcessor::getRepitch() const {
    return adapter.getRepitch();
}

int Op1CloneAudioProcessor::getStartPoint() const {
    return adapter.getStartPoint();
}

double Op1CloneAudioProcessor::getPlayheadPosition() const {
    return adapter.getPlayheadPosition();
}

float Op1CloneAudioProcessor::getEnvelopeValue() const {
    return adapter.getEnvelopeValue();
}

void Op1CloneAudioProcessor::getAllActivePlayheads(std::vector<double>& positions, std::vector<float>& envelopeValues) const {
    adapter.getAllActivePlayheads(positions, envelopeValues);
}

int Op1CloneAudioProcessor::getEndPoint() const {
    return adapter.getEndPoint();
}

float Op1CloneAudioProcessor::getSampleGain() const {
    return adapter.getSampleGain();
}

bool Op1CloneAudioProcessor::loadSampleFromFile(const juce::File& file) {
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"PluginProcessor.cpp:213\",\"message\":\"loadSampleFromFile entry\",\"data\":{\"fileExists\":" << (file.existsAsFile() ? 1 : 0) << ",\"fileName\":\"" << file.getFileName().toStdString() << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    if (!file.existsAsFile()) {
        return false;
    }
    
    // Use JUCE AudioFormatManager to load the file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"PluginProcessor.cpp:223\",\"message\":\"reader created\",\"data\":{\"readerNull\":" << (reader == nullptr ? 1 : 0) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    if (reader == nullptr) {
        return false;
    }
    
    // Create buffer for the sample
    juce::AudioBuffer<float> sampleBuffer(static_cast<int>(reader->numChannels), 
                                          static_cast<int>(reader->lengthInSamples));
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"PluginProcessor.cpp:230\",\"message\":\"buffer created\",\"data\":{\"numChannels\":" << static_cast<int>(reader->numChannels) << ",\"numSamples\":" << static_cast<int>(reader->lengthInSamples) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // Read the file into the buffer
    bool readSuccess = reader->read(&sampleBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"PluginProcessor.cpp:232\",\"message\":\"file read\",\"data\":{\"readSuccess\":" << (readSuccess ? 1 : 0) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    if (!readSuccess) {
        return false;
    }
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"PluginProcessor.cpp:237\",\"message\":\"calling adapter.setSample\",\"data\":{\"sampleRate\":" << reader->sampleRate << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // Pass to adapter (will extract mono from first channel)
    adapter.setSample(sampleBuffer, reader->sampleRate);
    
    // Also update slot 0 to keep it in sync with the default sample
    adapter.setSampleForSlot(0, sampleBuffer, reader->sampleRate);
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"PluginProcessor.cpp:240\",\"message\":\"loadSampleFromFile exit\",\"data\":{},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    return true;
}

void Op1CloneAudioProcessor::getSampleDataForVisualization(std::vector<float>& outData) const {
    adapter.getSampleDataForVisualization(outData);
}

void Op1CloneAudioProcessor::getStereoSampleDataForVisualization(std::vector<float>& outLeft, std::vector<float>& outRight) const {
    adapter.getStereoSampleDataForVisualization(outLeft, outRight);
}

void Op1CloneAudioProcessor::getSlotStereoSampleDataForVisualization(int slotIndex, std::vector<float>& outLeft, std::vector<float>& outRight) const {
    adapter.getSlotStereoSampleDataForVisualization(slotIndex, outLeft, outRight);
}

double Op1CloneAudioProcessor::getSourceSampleRate() const {
    return adapter.getSourceSampleRate();
}

double Op1CloneAudioProcessor::getSlotSourceSampleRate(int slotIndex) const {
    return adapter.getSlotSourceSampleRate(slotIndex);
}

std::array<bool, 5> Op1CloneAudioProcessor::getActiveSlots() const {
    return adapter.getActiveSlots();
}

int Op1CloneAudioProcessor::getActiveVoiceCount() const {
    return adapter.getActiveVoiceCount();
}

void Op1CloneAudioProcessor::setLPFilterCutoff(float cutoffHz) {
    adapter.setLPFilterCutoff(cutoffHz);
}

void Op1CloneAudioProcessor::setLPFilterResonance(float resonance) {
    adapter.setLPFilterResonance(resonance);
}

void Op1CloneAudioProcessor::setLPFilterEnvAmount(float amount) {
    adapter.setLPFilterEnvAmount(amount);
}

void Op1CloneAudioProcessor::setLPFilterDrive(float driveDb) {
    adapter.setLPFilterDrive(driveDb);
}

// Time warp speed removed - fixed at 1.0 (constant duration)

void Op1CloneAudioProcessor::setPlaybackMode(bool polyphonic) {
    adapter.setPlaybackMode(polyphonic);
}

void Op1CloneAudioProcessor::setPlaybackMode(int mode) {
    adapter.setPlaybackMode(mode);
}

void Op1CloneAudioProcessor::setSampleForSlot(int slotIndex, juce::AudioBuffer<float>& buffer, double sourceSampleRate) {
    adapter.setSampleForSlot(slotIndex, buffer, sourceSampleRate);
}

void Op1CloneAudioProcessor::setSlotRepitch(int slotIndex, float semitones) {
    adapter.setSlotRepitch(slotIndex, semitones);
}

void Op1CloneAudioProcessor::setSlotStartPoint(int slotIndex, int sampleIndex) {
    adapter.setSlotStartPoint(slotIndex, sampleIndex);
}

void Op1CloneAudioProcessor::setSlotEndPoint(int slotIndex, int sampleIndex) {
    adapter.setSlotEndPoint(slotIndex, sampleIndex);
}

void Op1CloneAudioProcessor::setSlotSampleGain(int slotIndex, float gain) {
    adapter.setSlotSampleGain(slotIndex, gain);
}

void Op1CloneAudioProcessor::setSlotADSR(int slotIndex, float attackMs, float decayMs, float sustain, float releaseMs) {
    adapter.setSlotADSR(slotIndex, attackMs, decayMs, sustain, releaseMs);
}

void Op1CloneAudioProcessor::setSlotLoopEnabled(int slotIndex, bool enabled) {
    adapter.setSlotLoopEnabled(slotIndex, enabled);
}

void Op1CloneAudioProcessor::setSlotLoopPoints(int slotIndex, int startPoint, int endPoint) {
    adapter.setSlotLoopPoints(slotIndex, startPoint, endPoint);
}

float Op1CloneAudioProcessor::getSlotRepitch(int slotIndex) const {
    return adapter.getSlotRepitch(slotIndex);
}

int Op1CloneAudioProcessor::getSlotStartPoint(int slotIndex) const {
    return adapter.getSlotStartPoint(slotIndex);
}

int Op1CloneAudioProcessor::getSlotEndPoint(int slotIndex) const {
    return adapter.getSlotEndPoint(slotIndex);
}

float Op1CloneAudioProcessor::getSlotSampleGain(int slotIndex) const {
    return adapter.getSlotSampleGain(slotIndex);
}

void Op1CloneAudioProcessor::setLoopEnabled(bool enabled) {
    adapter.setLoopEnabled(enabled);
}

void Op1CloneAudioProcessor::setLoopPoints(int startPoint, int endPoint) {
    adapter.setLoopPoints(startPoint, endPoint);
}

void Op1CloneAudioProcessor::setWarpEnabled(bool enabled) {
    adapter.setWarpEnabled(enabled);
}

void Op1CloneAudioProcessor::setTimeRatio(double ratio) {
    adapter.setTimeRatio(ratio);
}

void Op1CloneAudioProcessor::setFilterEffectsEnabled(bool enabled) {
    adapter.setFilterEffectsEnabled(enabled);
}

void Op1CloneAudioProcessor::setLoopEnvAttack(float attackMs) {
    adapter.setLoopEnvAttack(attackMs);
}

void Op1CloneAudioProcessor::setLoopEnvRelease(float releaseMs) {
    adapter.setLoopEnvRelease(releaseMs);
}

void Op1CloneAudioProcessor::triggerTestNote() {
    // Thread-safe lock-free: queue MIDI message for next audio block
    sendMidiNote(60, 1.0f, true);
}

void Op1CloneAudioProcessor::sendMidiNote(int note, float velocity, bool noteOn) {
    // Thread-safe lock-free: queue MIDI message for next audio block
    int currentCount = pendingMidiCount.load(std::memory_order_acquire);
    if (currentCount < 32) {
        if (noteOn) {
            midiMessageBuffer[currentCount] = juce::MidiMessage::noteOn(1, note, velocity);
        } else {
            midiMessageBuffer[currentCount] = juce::MidiMessage::noteOff(1, note, velocity);
        }
        pendingMidiCount.store(currentCount + 1, std::memory_order_release);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new Op1CloneAudioProcessor();
}

