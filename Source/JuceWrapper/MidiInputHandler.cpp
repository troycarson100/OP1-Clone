#include "MidiInputHandler.h"
#include <algorithm>

MidiInputHandler::MidiInputHandler()
    : fifo(FIFO_SIZE)
{
}

MidiInputHandler::~MidiInputHandler()
{
}

void MidiInputHandler::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    // Called from MIDI thread - must be lock-free
    // Ignore active sense messages
    if (message.isActiveSense())
        return;
    
    // Try to write to FIFO
    int start1, size1, start2, size2;
    fifo.prepareToWrite(1, start1, size1, start2, size2);
    
    if (size1 > 0)
    {
        messageBuffer[start1].message = message;
        messageBuffer[start1].timestamp = juce::Time::getMillisecondCounterHiRes();
        fifo.finishedWrite(1);
        
        // Update last note info for UI (atomic operations)
        // Handle note-on with velocity 0 as note-off (MIDI standard)
        if (message.isNoteOn())
        {
            if (message.getVelocity() == 0)
            {
                // Note-on with velocity 0 is actually note-off
                lastNote.store(message.getNoteNumber(), std::memory_order_relaxed);
                lastVelocity.store(0.0f, std::memory_order_relaxed);
                lastIsNoteOn.store(false, std::memory_order_relaxed);
            }
            else
            {
                lastNote.store(message.getNoteNumber(), std::memory_order_relaxed);
                lastVelocity.store(message.getFloatVelocity(), std::memory_order_relaxed);
                lastIsNoteOn.store(true, std::memory_order_relaxed);
            }
        }
        else if (message.isNoteOff())
        {
            lastNote.store(message.getNoteNumber(), std::memory_order_relaxed);
            lastVelocity.store(0.0f, std::memory_order_relaxed);
            lastIsNoteOn.store(false, std::memory_order_relaxed);
        }
    }
    // If FIFO is full, we drop the message (better than blocking)
}

void MidiInputHandler::drainToMidiBuffer(juce::MidiBuffer& midiBuffer, int numSamples)
{
    // Called from audio thread - must be lock-free
    int start1, size1, start2, size2;
    fifo.prepareToRead(fifo.getNumReady(), start1, size1, start2, size2);
    
    // Read first block
    for (int i = 0; i < size1; ++i)
    {
        const auto& msgWithTime = messageBuffer[start1 + i];
        // Add message at sample position 0 (we could use timestamp for more accuracy)
        midiBuffer.addEvent(msgWithTime.message, 0);
    }
    
    // Read second block (if FIFO wrapped around)
    for (int i = 0; i < size2; ++i)
    {
        const auto& msgWithTime = messageBuffer[start2 + i];
        midiBuffer.addEvent(msgWithTime.message, 0);
    }
    
    fifo.finishedRead(size1 + size2);
}

MidiInputHandler::LastNoteInfo MidiInputHandler::getLastNoteInfo() const
{
    LastNoteInfo info;
    info.note = lastNote.load(std::memory_order_acquire);
    info.velocity = lastVelocity.load(std::memory_order_acquire);
    info.isNoteOn = lastIsNoteOn.load(std::memory_order_acquire);
    return info;
}

juce::StringArray MidiInputHandler::getEnabledDeviceNames() const
{
    juce::CriticalSection::ScopedLockType lock(deviceNamesLock);
    return enabledDeviceNames;
}

void MidiInputHandler::setEnabledDeviceNames(const juce::StringArray& names)
{
    juce::CriticalSection::ScopedLockType lock(deviceNamesLock);
    enabledDeviceNames = names;
}

