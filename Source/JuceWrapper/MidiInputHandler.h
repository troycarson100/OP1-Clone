#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

/**
 * Lock-free MIDI input handler for standalone app
 * Uses AbstractFifo with pre-allocated buffer for thread-safe MIDI queuing
 */
class MidiInputHandler : public juce::MidiInputCallback
{
public:
    static constexpr int FIFO_SIZE = 128;
    
    MidiInputHandler();
    ~MidiInputHandler() override;
    
    // MidiInputCallback interface - called from MIDI thread
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    
    // Drain FIFO into MidiBuffer (called from audio thread)
    void drainToMidiBuffer(juce::MidiBuffer& midiBuffer, int numSamples);
    
    // Get last received note info for UI (thread-safe)
    struct LastNoteInfo {
        int note = -1;
        float velocity = 0.0f;
        bool isNoteOn = false;
    };
    LastNoteInfo getLastNoteInfo() const;
    
    // Get enabled device names for UI (thread-safe)
    juce::StringArray getEnabledDeviceNames() const;
    
    // Set enabled device names (called from message thread)
    void setEnabledDeviceNames(const juce::StringArray& names);

private:
    // Lock-free FIFO for MIDI messages
    juce::AbstractFifo fifo;
    
    // Pre-allocated buffer for MIDI messages
    struct MidiMessageWithTime {
        juce::MidiMessage message;
        int64_t timestamp;
    };
    std::array<MidiMessageWithTime, FIFO_SIZE> messageBuffer;
    
    // Last note info for UI (atomic for thread safety)
    std::atomic<int> lastNote{-1};
    std::atomic<float> lastVelocity{0.0f};
    std::atomic<bool> lastIsNoteOn{false};
    
    // Enabled device names (protected by simple atomic flag)
    juce::StringArray enabledDeviceNames;
    mutable juce::CriticalSection deviceNamesLock;
};

