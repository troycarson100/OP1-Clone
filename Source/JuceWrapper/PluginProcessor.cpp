#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <cmath>

// For now, we'll generate a simple test tone as the default sample
// In a real implementation, you'd load a WAV file from BinaryData
static void generateTestTone(juce::AudioBuffer<float>& buffer, double sampleRate, float frequency, float duration) {
    int numSamples = static_cast<int>(sampleRate * duration);
    buffer.setSize(1, numSamples, false, true, false);
    
    float* channelData = buffer.getWritePointer(0);
    float phase = 0.0f;
    float phaseIncrement = static_cast<float>(2.0 * juce::MathConstants<double>::pi * frequency / sampleRate);
    
    for (int i = 0; i < numSamples; ++i) {
        channelData[i] = std::sin(phase) * 0.5f;
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
    
    // Process through adapter
    adapter.processBlock(buffer, midiMessages);
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
    
    // Pass to adapter
    adapter.setSample(testBuffer, 44100.0);
}

bool Op1CloneAudioProcessor::loadSampleFromFile(const juce::File& file) {
    if (!file.existsAsFile()) {
        return false;
    }
    
    // Use JUCE AudioFormatManager to load the file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr) {
        return false;
    }
    
    // Create buffer for the sample
    juce::AudioBuffer<float> sampleBuffer(static_cast<int>(reader->numChannels), 
                                          static_cast<int>(reader->lengthInSamples));
    
    // Read the file into the buffer
    if (!reader->read(&sampleBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true)) {
        return false;
    }
    
    // Pass to adapter (will extract mono from first channel)
    adapter.setSample(sampleBuffer, reader->sampleRate);
    
    return true;
}

void Op1CloneAudioProcessor::getSampleDataForVisualization(std::vector<float>& outData) const {
    adapter.getSampleDataForVisualization(outData);
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

