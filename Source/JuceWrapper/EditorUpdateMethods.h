#pragma once

#include <juce_core/juce_core.h>
#include <vector>

class Op1CloneAudioProcessorEditor;

// Manager class to handle all update/refresh methods
// Extracted from PluginEditor to comply with 500-line file rule
class EditorUpdateMethods {
public:
    EditorUpdateMethods(Op1CloneAudioProcessorEditor* editor);
    
    // Update waveform visualization
    void updateWaveform();
    
    // Update ADSR visualization and send to processor
    void updateADSR();
    
    // Update parameter display text
    void updateParameterDisplay(const juce::String& paramName, float valueMs);
    
    // Update sample editing parameters and send to processor
    void updateSampleEditing();
    
    // Update waveform visualization with current parameters
    void updateWaveformVisualization();
    
    // Update BPM display in top right corner
    void updateBPMDisplay();
    
    // Update parameter display labels based on shift state
    void updateParameterDisplayLabels();
    
    // Update loop controls state (grey out when disabled)
    void updateLoopControlsState();
    
    // Update shift mode display values when shift is toggled
    void updateShiftModeDisplayValues();
    
private:
    Op1CloneAudioProcessorEditor* editor;
};

