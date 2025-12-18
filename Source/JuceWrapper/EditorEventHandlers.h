#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class Op1CloneAudioProcessorEditor;

// Manager class to handle keyboard and button events
// Extracted from PluginEditor to comply with 500-line file rule
class EditorEventHandlers {
public:
    EditorEventHandlers(Op1CloneAudioProcessorEditor* editor);
    
    // Handle keyboard events
    bool handleKeyPressed(const juce::KeyPress& key);
    bool handleKeyStateChanged(bool isKeyDown);
    
    // Handle button clicks
    void handleButtonClicked(juce::Button* button);
    
    // Handle load sample button
    void handleLoadSampleButtonClicked();
    
    // Helper methods (public for const access from PluginEditor)
    int keyToMidiNote(int keyCode) const;
    void sendMidiNote(int note, float velocity, bool noteOn);
    
private:
    Op1CloneAudioProcessorEditor* editor;
};

