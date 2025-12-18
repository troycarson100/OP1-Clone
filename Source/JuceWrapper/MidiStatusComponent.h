#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MidiInputHandler.h"

/**
 * UI component to display MIDI input status
 * Safe for message thread only - reads atomic values from MidiInputHandler
 */
class MidiStatusComponent : public juce::Component, private juce::Timer
{
public:
    MidiStatusComponent();
    ~MidiStatusComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Set the MIDI handler to monitor (called from message thread)
    void setMidiHandler(MidiInputHandler* handler);
    
private:
    void timerCallback() override;
    
    MidiInputHandler* midiHandler = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiStatusComponent)
};

