#pragma once

class Op1CloneAudioProcessorEditor;

// Manager class to handle timer-based fade-out logic
// Extracted from PluginEditor to comply with 500-line file rule
class EditorTimerCallback {
public:
    EditorTimerCallback(Op1CloneAudioProcessorEditor* editor);
    
    // Handle timer callback for fade-out animations
    void handleTimerCallback();
    
private:
    Op1CloneAudioProcessorEditor* editor;
};

