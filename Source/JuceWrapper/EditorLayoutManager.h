#pragma once

class Op1CloneAudioProcessorEditor;

// Manager class to handle all layout calculations
// Extracted from PluginEditor to comply with 500-line file rule
class EditorLayoutManager {
public:
    EditorLayoutManager(Op1CloneAudioProcessorEditor* editor);
    
    // Calculate and apply layout for all components
    void layoutComponents();
    
private:
    Op1CloneAudioProcessorEditor* editor;
};




