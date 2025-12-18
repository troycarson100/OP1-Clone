#include "MidiStatusComponent.h"

MidiStatusComponent::MidiStatusComponent()
{
    // Update UI every 50ms (20fps) - safe for message thread
    startTimer(50);
}

MidiStatusComponent::~MidiStatusComponent()
{
    stopTimer();
}

void MidiStatusComponent::setMidiHandler(MidiInputHandler* handler)
{
    midiHandler = handler;
}

void MidiStatusComponent::timerCallback()
{
    // Trigger repaint to update MIDI status
    repaint();
}

void MidiStatusComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
    
    if (midiHandler == nullptr)
    {
        g.setColour(juce::Colours::grey);
        g.setFont(12.0f);
        g.drawText("MIDI: No handler", getLocalBounds(), juce::Justification::centredLeft);
        return;
    }
    
    // Get device names (thread-safe)
    auto deviceNames = midiHandler->getEnabledDeviceNames();
    
    // Get last note info (thread-safe atomic reads)
    auto lastNote = midiHandler->getLastNoteInfo();
    
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    
    juce::String statusText;
    
    if (deviceNames.size() > 0)
    {
        statusText = "MIDI: " + deviceNames.joinIntoString(", ");
    }
    else
    {
        statusText = "MIDI: No devices";
    }
    
    if (lastNote.note >= 0)
    {
        statusText += " | Last: ";
        if (lastNote.isNoteOn)
        {
            statusText += "Note " + juce::String(lastNote.note) + 
                         " On (vel " + juce::String(lastNote.velocity, 2) + ")";
        }
        else
        {
            statusText += "Note " + juce::String(lastNote.note) + " Off";
        }
    }
    
    g.drawText(statusText, getLocalBounds().reduced(4), juce::Justification::centredLeft);
}

void MidiStatusComponent::resized()
{
    // Component fills its bounds
}

