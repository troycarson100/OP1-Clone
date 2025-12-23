#include "EditorUpdateMethods.h"
#include "PluginEditor.h"
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <algorithm>
#include <cmath>

EditorUpdateMethods::EditorUpdateMethods(Op1CloneAudioProcessorEditor* editor)
    : editor(editor)
{
}

void EditorUpdateMethods::updateWaveform() {
    // Get sample data from processor and update screen
    std::vector<float> sampleData;
    editor->audioProcessor.getSampleDataForVisualization(sampleData);
    
    // Safety check: if no sample data, just clear the screen and return early
    if (sampleData.empty()) {
        editor->screenComponent.setSampleData(sampleData);
        return;
    }
    
    editor->screenComponent.setSampleData(sampleData);
    
    // Get source sample rate from processor (for time calculations)
    double sampleRate = editor->audioProcessor.getSourceSampleRate();
    if (sampleRate > 0.0) {
        editor->sampleRate = sampleRate;
    }
    
    // Ensure ADSR visualization is hidden on startup/update
    // Remove from component tree if it was added
    if (editor->adsrVisualization.getParentComponent() != nullptr) {
        editor->adsrVisualization.getParentComponent()->removeChildComponent(&editor->adsrVisualization);
    }
    editor->adsrVisualization.setAlpha(0.0f);
    editor->adsrVisualization.setVisible(false);
    editor->isADSRDragging = false;
    editor->adsrFadeOutStartTime = 0;
    
    // Update sample length for encoder mapping
    int newSampleLength = static_cast<int>(sampleData.size());
    
    // If this is a new sample (different length) or first load, reset to full sample view
    bool isNewSample = (newSampleLength != editor->sampleLength);
    
    if (newSampleLength > 0) {
        if (isNewSample || editor->endPoint == 0 || editor->endPoint > newSampleLength || editor->startPoint >= editor->endPoint) {
            // Reset to show full sample
            editor->startPoint = 0;
            editor->endPoint = newSampleLength;
            
            // Update encoder positions to match (encoder2 = 0.0 for start, encoder3 = 1.0 for end)
            editor->encoder2.setValue(0.0f);
            editor->encoder3.setValue(1.0f);
        }
        
        editor->sampleLength = newSampleLength;
        
        // Update the processor with initial values
        updateSampleEditing();
        updateWaveformVisualization();
        
        // Initialize parameter displays with current values
        float normalizedPitch = (editor->repitchSemitones + 24.0f) / 48.0f;
        editor->paramDisplay1.setValue(normalizedPitch);
        int semitones = static_cast<int>(std::round(editor->repitchSemitones));
        editor->paramDisplay1.setValueText((semitones >= 0 ? "+" : "") + juce::String(semitones));
        
        float startValue = editor->startPoint > 0 ? static_cast<float>(editor->startPoint) / static_cast<float>(editor->sampleLength) : 0.0f;
        editor->paramDisplay2.setValue(startValue);
        double startTimeSeconds = static_cast<double>(editor->startPoint) / editor->sampleRate;
        if (startTimeSeconds >= 1.0) {
            editor->paramDisplay2.setValueText(juce::String(startTimeSeconds, 2) + "s");
        } else {
            editor->paramDisplay2.setValueText(juce::String(static_cast<int>(startTimeSeconds * 1000.0)) + "ms");
        }
        
        float endValue = editor->endPoint > 0 ? static_cast<float>(editor->endPoint) / static_cast<float>(editor->sampleLength) : 1.0f;
        editor->paramDisplay3.setValue(endValue);
        double endTimeSeconds = static_cast<double>(editor->endPoint) / editor->sampleRate;
        if (endTimeSeconds >= 1.0) {
            editor->paramDisplay3.setValueText(juce::String(endTimeSeconds, 2) + "s");
        } else {
            editor->paramDisplay3.setValueText(juce::String(static_cast<int>(endTimeSeconds * 1000.0)) + "ms");
        }
        
        editor->paramDisplay4.setValue(editor->sampleGain / 2.0f);
        editor->paramDisplay4.setValueText(juce::String(editor->sampleGain, 2) + "x");
        
        float attackValue = editor->adsrAttackMs / 10000.0f;
        editor->paramDisplay5.setValue(attackValue);
        if (editor->adsrAttackMs >= 1000.0f) {
            editor->paramDisplay5.setValueText(juce::String(editor->adsrAttackMs / 1000.0f, 2) + "s");
        } else {
            editor->paramDisplay5.setValueText(juce::String(static_cast<int>(editor->adsrAttackMs)) + "ms");
        }
        
        float decayValue = editor->adsrDecayMs / 20000.0f;
        editor->paramDisplay6.setValue(decayValue);
        if (editor->adsrDecayMs >= 1000.0f) {
            editor->paramDisplay6.setValueText(juce::String(editor->adsrDecayMs / 1000.0f, 2) + "s");
        } else {
            editor->paramDisplay6.setValueText(juce::String(static_cast<int>(editor->adsrDecayMs)) + "ms");
        }
        
        editor->paramDisplay7.setValue(editor->adsrSustain);
        editor->paramDisplay7.setValueText(juce::String(static_cast<int>(editor->adsrSustain * 100.0f)) + "%");
        
        float releaseValue = editor->adsrReleaseMs / 20000.0f;
        editor->paramDisplay8.setValue(releaseValue);
        if (editor->adsrReleaseMs >= 1000.0f) {
            editor->paramDisplay8.setValueText(juce::String(editor->adsrReleaseMs / 1000.0f, 2) + "s");
        } else {
            editor->paramDisplay8.setValueText(juce::String(static_cast<int>(editor->adsrReleaseMs)) + "ms");
        }
    }
}

void EditorUpdateMethods::updateADSR() {
    // Update visualization (only if explicitly being dragged/interacted with)
    // Don't update visualization on startup or when hidden
    if (editor->isADSRDragging) {
        editor->adsrVisualization.setADSR(editor->adsrAttackMs, editor->adsrDecayMs, editor->adsrSustain, editor->adsrReleaseMs);
    }
    
    // Send to processor (always update the audio processor)
    editor->audioProcessor.setADSR(editor->adsrAttackMs, editor->adsrDecayMs, editor->adsrSustain, editor->adsrReleaseMs);
}

void EditorUpdateMethods::updateParameterDisplay(const juce::String& paramName, float value) {
    // Format the value appropriately
    juce::String valueText;
    if (paramName == "S") {
        // Sustain is a percentage
        valueText = juce::String(value, 0) + "%";
    } else if (paramName == "Pitch") {
        // Pitch is in semitones (show with +/- sign, rounded to nearest semitone)
        int semitones = static_cast<int>(std::round(value));
        if (semitones >= 0) {
            valueText = "+" + juce::String(semitones) + "st";
        } else {
            valueText = juce::String(semitones) + "st";
        }
    } else {
        // Attack, Decay, Release are in milliseconds
        if (value >= 1000.0f) {
            // Show as seconds if >= 1 second
            float seconds = value / 1000.0f;
            valueText = juce::String(seconds, 1) + "s";
        } else {
            // Show as milliseconds
            valueText = juce::String(static_cast<int>(value)) + "ms";
        }
    }
    
    // Update text
    editor->currentParameterText = paramName + " " + valueText;
    editor->parameterDisplayLabel.setText(editor->currentParameterText, juce::dontSendNotification);
    
    // Reset fade-out timer
    editor->lastEncoderChangeTime = juce::Time::currentTimeMillis();
    editor->parameterDisplayAlpha = 1.0f;
    editor->parameterDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    editor->parameterDisplayLabel.setVisible(true);
    editor->parameterDisplayLabel.repaint();
}

void EditorUpdateMethods::updateSampleEditing() {
    // Send sample editing parameters to processor
    editor->audioProcessor.setRepitch(editor->repitchSemitones);
    editor->audioProcessor.setStartPoint(editor->startPoint);
    editor->audioProcessor.setEndPoint(editor->endPoint);
    editor->audioProcessor.setSampleGain(editor->sampleGain);
}

void EditorUpdateMethods::updateWaveformVisualization() {
    // Update waveform component with current start/end points and gain
    editor->screenComponent.setStartPoint(editor->startPoint);
    editor->screenComponent.setEndPoint(editor->endPoint);
    editor->screenComponent.setSampleGain(editor->sampleGain);
    // Update loop markers
    editor->screenComponent.setLoopStartPoint(editor->loopStartPoint);
    editor->screenComponent.setLoopEndPoint(editor->loopEndPoint);
    editor->screenComponent.setLoopEnabled(editor->loopEnabled);
    // Force repaint to show the zoomed waveform
    editor->screenComponent.repaint();
}

void EditorUpdateMethods::updateBPMDisplay() {
    // Show BPM value in top right corner (e.g., "BPM: 120")
    juce::String bpmText = "BPM: " + juce::String(editor->projectBPM);
    editor->bpmDisplayLabel.setText(bpmText, juce::dontSendNotification);
    editor->bpmDisplayLabel.repaint();
}

void EditorUpdateMethods::updateParameterDisplayLabels() {
    // Update parameter display labels based on shift state
    bool shiftEnabled = editor->shiftToggleButton.getToggleState();
    
    if (shiftEnabled) {
        // Shift mode: LP filter and loop parameters
        editor->paramDisplay1.setLabel("CUTOFF");
        editor->paramDisplay2.setLabel("RES.");
        editor->paramDisplay3.setLabel("DRIVE");
        editor->paramDisplay4.setLabel("SPEED");
        editor->paramDisplay5.setLabel("L.START");
        editor->paramDisplay6.setLabel("L.END");
        editor->paramDisplay7.setLabel("LOOP");
        editor->paramDisplay8.setLabel("PLAY");
    } else {
        // Normal mode: Pitch, Start, End, Gain, ADSR
        editor->paramDisplay1.setLabel("PITCH");
        editor->paramDisplay2.setLabel("START");
        editor->paramDisplay3.setLabel("END");
        editor->paramDisplay4.setLabel("GAIN");
        editor->paramDisplay5.setLabel("ATTACK");
        editor->paramDisplay6.setLabel("DECAY");
        editor->paramDisplay7.setLabel("SUSTAIN");
        editor->paramDisplay8.setLabel("RELEASE");
    }
}

void EditorUpdateMethods::updateLoopControlsState() {
    // Grey out loop controls when loop is disabled
    bool loopEnabled = editor->loopEnabled;
    juce::Colour enabledColor = juce::Colours::white;
    juce::Colour disabledColor = juce::Colours::grey;
    
    // Only update if in shift mode
    if (editor->shiftToggleButton.getToggleState()) {
        // Update loop start/end displays (5, 6) - grey out when disabled
        editor->paramDisplay5.setColour(juce::Label::textColourId, loopEnabled ? enabledColor : disabledColor);
        editor->paramDisplay6.setColour(juce::Label::textColourId, loopEnabled ? enabledColor : disabledColor);
        // Loop toggle itself (7) is always enabled (white)
        editor->paramDisplay7.setColour(juce::Label::textColourId, enabledColor);
    }
}

void EditorUpdateMethods::updateShiftModeDisplayValues() {
    // Update display values when shift mode is toggled
    bool shiftEnabled = editor->shiftToggleButton.getToggleState();
    
    if (shiftEnabled) {
        // Shift mode: Update displays with current shift mode values
        // Encoder 1: Cutoff (20-20000 Hz, default 20kHz = 1.0)
        float cutoffValue = std::log(editor->lpCutoffHz / 20.0f) / std::log(1000.0f);
        cutoffValue = std::max(0.0f, std::min(1.0f, cutoffValue));
        editor->encoder1.setValue(cutoffValue);
        editor->paramDisplay1.setValue(cutoffValue);
        if (editor->lpCutoffHz >= 1000.0f) {
            editor->paramDisplay1.setValueText(juce::String(editor->lpCutoffHz / 1000.0f, 1) + "kHz");
        } else {
            editor->paramDisplay1.setValueText(juce::String(static_cast<int>(editor->lpCutoffHz)) + "Hz");
        }
        
        // Encoder 2: Resonance (0.0-4.0, default 1.0 = 0.25)
        float resValue = editor->lpResonance / 4.0f;
        editor->encoder2.setValue(resValue);
        editor->paramDisplay2.setValue(resValue);
        editor->paramDisplay2.setValueText(juce::String(editor->lpResonance, 2));
        
        // Encoder 3: Drive (0-20 dB, default 0 dB = 0.0)
        float driveValue = editor->lpDriveDb / 20.0f;
        editor->encoder3.setValue(driveValue);
        editor->paramDisplay3.setValue(driveValue);
        editor->paramDisplay3.setValueText(juce::String(editor->lpDriveDb, 1) + "dB");
        
        // Encoder 4: Speed (time warp, 0.25-4.0x, default 1.0x = 0.5)
        float speedValue = (editor->timeWarpSpeed - 0.25f) / 3.75f;
        editor->encoder4.setValue(speedValue);
        editor->paramDisplay4.setValue(speedValue);
        editor->paramDisplay4.setValueText(juce::String(editor->timeWarpSpeed, 2) + "x");
        
        // Encoder 5: Loop Start (already handled by waveform)
        // Encoder 6: Loop End (already handled by waveform)
        
        // Encoder 7: Loop (ON/OFF, default OFF = 0.0)
        float loopValue = editor->loopEnabled ? 1.0f : 0.0f;
        editor->encoder7.setValue(loopValue);
        editor->paramDisplay7.setValue(loopValue);
        editor->paramDisplay7.setValueText(editor->loopEnabled ? "ON" : "OFF");
        
        // Encoder 8: Play (Mono/Poly, default Poly = 1.0)
        float playValue = editor->isPolyphonic ? 1.0f : 0.0f;
        editor->encoder8.setValue(playValue);
        editor->paramDisplay8.setValue(playValue);
        editor->paramDisplay8.setValueText(editor->isPolyphonic ? "Poly" : "Mono");
    }
}

