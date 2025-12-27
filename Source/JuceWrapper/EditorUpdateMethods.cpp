#include "EditorUpdateMethods.h"
#include "PluginEditor.h"
#include "EncoderSetupManager.h"
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
    // Use current slot index
    updateWaveform(editor->currentSlotIndex);
}

void EditorUpdateMethods::updateWaveform(int slotIndex) {
    // Get stereo sample data from processor for the specified slot
    std::vector<float> leftChannel, rightChannel;
    editor->audioProcessor.getSlotStereoSampleDataForVisualization(slotIndex, leftChannel, rightChannel);
    
    // Safety check: if no sample data, just clear the screen and return early
    if (leftChannel.empty()) {
        // Don't clear the waveform if we're just updating parameters for a slot that has no sample
        // Only clear if this is a different slot than current
        if (slotIndex == editor->currentSlotIndex) {
            editor->screenComponent.setSampleData(leftChannel);
        }
        // Clear preview for this slot if no sample
        editor->screenComponent.setSlotPreview(slotIndex, std::vector<float>());
        return;
    }
    
    // Use stereo data if right channel is available, otherwise use mono
    // Always update the waveform data, even if it's the same slot
    // This ensures the waveform is visible after loading or switching slots
    if (!rightChannel.empty()) {
        editor->screenComponent.setStereoSampleData(leftChannel, rightChannel);
    } else {
        editor->screenComponent.setSampleData(leftChannel);
    }
    
    // CRITICAL: Ensure endPoint is set to sample length so waveform is visible
    // If endPoint is 0, the waveform won't render (visibleLength will be 0)
    if (!leftChannel.empty() && editor->endPoint == 0) {
        editor->endPoint = static_cast<int>(leftChannel.size());
        editor->screenComponent.setEndPoint(editor->endPoint);
    }
    
    // Force immediate repaint to ensure waveform is displayed
    editor->screenComponent.repaint();
    
    // Update sample slot preview for the specified slot with left channel data
    if (!leftChannel.empty()) {
        // For preview, we can use more samples for better detail
        // Use a reasonable downsample factor to get enough points for a good preview
        std::vector<float> previewData;
        int targetPoints = 400;  // More points for better waveform detail
        int downsampleFactor = static_cast<int>(leftChannel.size() / targetPoints);
        if (downsampleFactor < 1) downsampleFactor = 1;
        for (size_t i = 0; i < leftChannel.size(); i += downsampleFactor) {
            previewData.push_back(leftChannel[i]);
        }
        // Update preview for the specified slot (not just slot A)
        editor->screenComponent.setSlotPreview(slotIndex, previewData);
    }
}

void EditorUpdateMethods::updateAllSlotPreviews() {
    // Update previews for all slots (0-4 for A-E)
    for (int slotIndex = 0; slotIndex < 5; ++slotIndex) {
        std::vector<float> leftChannel, rightChannel;
        editor->audioProcessor.getSlotStereoSampleDataForVisualization(slotIndex, leftChannel, rightChannel);
        
        if (!leftChannel.empty()) {
            // For preview, we can use more samples for better detail
            // Use a reasonable downsample factor to get enough points for a good preview
            std::vector<float> previewData;
            int targetPoints = 400;  // More points for better waveform detail
            int downsampleFactor = static_cast<int>(leftChannel.size() / targetPoints);
            if (downsampleFactor < 1) downsampleFactor = 1;
            for (size_t i = 0; i < leftChannel.size(); i += downsampleFactor) {
                previewData.push_back(leftChannel[i]);
            }
            // Update preview for this slot
            editor->screenComponent.setSlotPreview(slotIndex, previewData);
        } else {
            // Clear preview for slots without samples
            editor->screenComponent.setSlotPreview(slotIndex, std::vector<float>());
        }
    }
}

void EditorUpdateMethods::updateADSR() {
    // Update pill visualization (always visible, always update)
    editor->adsrPillComponent.setADSR(editor->adsrAttackMs, editor->adsrDecayMs, editor->adsrSustain, editor->adsrReleaseMs);
    
    // OLD visualization (COMMENTED OUT)
    /*
    // Update visualization (only if explicitly being dragged/interacted with)
    // Don't update visualization on startup or when hidden
    if (editor->isADSRDragging) {
        editor->adsrVisualization.setADSR(editor->adsrAttackMs, editor->adsrDecayMs, editor->adsrSustain, editor->adsrReleaseMs);
    }
    */
    
    // NOTE: We no longer update global engine parameters here because we use per-slot parameters
    // The encoder callbacks update the current slot's parameters directly via setSlotADSR
    // This prevents parameters from one slot from affecting voices playing from other slots
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
    // NOTE: We no longer update global engine parameters here because we use per-slot parameters
    // The encoder callbacks update the current slot's parameters directly via setSlotRepitch/etc.
    // This prevents parameters from one slot from affecting voices playing from other slots
    // This method is kept for backward compatibility but does nothing now
}

void EditorUpdateMethods::updateWaveformVisualization() {
    // Always update the waveform data to ensure it's current for the selected slot
    // This handles both slot switching and parameter updates
    updateWaveform(editor->currentSlotIndex);
    
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
    editor->repaint();  // Also repaint the editor to ensure everything is updated
}

void EditorUpdateMethods::updateBPMDisplay() {
    // Show BPM value in top right corner (e.g., "BPM: 120")
    juce::String bpmText = "BPM: " + juce::String(editor->projectBPM);
    editor->bpmDisplayLabel.setText(bpmText, juce::dontSendNotification);
    editor->bpmDisplayLabel.repaint();
}

void EditorUpdateMethods::updateParameterDisplayLabels() {
    // Check if orbit menu is open - if so, use orbit-specific labels
    if (editor->playbackMode == 2 && editor->orbitMenuOpen) {
        // Orbit menu mode: Orbit-specific controls
        // TRIPLET and DOTTED are shown as part of RATE value, not separate knobs
        editor->paramDisplay1.setLabel("RATE");
        editor->paramDisplay2.setLabel("SHAPE");
        editor->paramDisplay3.setLabel("CURVE");
        // Clear knobs 4-8 (blank labels and empty values)
        editor->paramDisplay4.setLabel("");
        editor->paramDisplay4.setValueText("");
        editor->paramDisplay4.setValue(0.0f);
        editor->paramDisplay5.setLabel("");
        editor->paramDisplay5.setValueText("");
        editor->paramDisplay5.setValue(0.0f);
        editor->paramDisplay6.setLabel("");
        editor->paramDisplay6.setValueText("");
        editor->paramDisplay6.setValue(0.0f);
        editor->paramDisplay7.setLabel("");
        editor->paramDisplay7.setValueText("");
        editor->paramDisplay7.setValue(0.0f);
        editor->paramDisplay8.setLabel("");
        editor->paramDisplay8.setValueText("");
        editor->paramDisplay8.setValue(0.0f);
        
        // Update display values and sync encoder positions for first 3 knobs only
        juce::String rateText = EncoderSetupManager::getNoteValueString(editor->orbitRateNoteValue);
        if (editor->orbitRateTriplet) rateText += "T";
        if (editor->orbitRateDotted) rateText += ".";
        // Reverse mapping: noteValue 0=4 bars (encoder 1.0), noteValue 8=1/64 (encoder 0.0)
        float reversedValue = 1.0f - (editor->orbitRateNoteValue / 8.0f);
        editor->paramDisplay1.setValue(reversedValue);
        editor->paramDisplay1.setValueText(rateText);
        editor->encoder1.setValue(reversedValue);  // Sync encoder position
        
        // Update shape display
        const char* shapeNames[] = {"Circle", "PingPong", "Corners", "Random", "Figure8", "ZigZag", "Spiral", "Square"};
        if (editor->orbitShape >= 0 && editor->orbitShape < 8) {
            editor->paramDisplay2.setValueText(shapeNames[editor->orbitShape]);
        }
        editor->paramDisplay2.setValue(editor->orbitShape / 7.0f);  // 0-7 maps to 0.0-1.0
        editor->encoder2.setValue(editor->orbitShape / 7.0f);  // Sync encoder position
        
        // Update curve display
        editor->paramDisplay3.setValueText(juce::String(static_cast<int>(editor->orbitCurve * 100)) + "%");
        editor->paramDisplay3.setValue(editor->orbitCurve);
        editor->encoder3.setValue(editor->orbitCurve);  // Sync encoder position
        
        return;
    }
    
    // Update parameter display labels based on shift state
    // But ignore shift mode if orbit menu is open
    bool shiftEnabled = editor->shiftToggleButton.getToggleState();
    
    if (shiftEnabled && !(editor->playbackMode == 2 && editor->orbitMenuOpen)) {
        // Shift mode: LP filter and loop parameters
        editor->paramDisplay1.setLabel("CUTOFF");
        editor->paramDisplay2.setLabel("RES.");
        editor->paramDisplay3.setLabel("DRIVE");
        editor->paramDisplay4.setLabel("P.B.");  // Playback mode (Stacked/Round Robin/Orbit)
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
    // But ignore shift mode if orbit menu is open
    if (editor->playbackMode == 2 && editor->orbitMenuOpen) {
        // Orbit menu is open - don't show shift mode values
        return;
    }
    
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
        
        // Encoder 4: Playback mode (0 = Stacked, 1 = Round Robin, 2 = Orbit, default 0 = 0.0)
        float playbackValue = editor->playbackMode == 0 ? 0.0f : (editor->playbackMode == 1 ? 0.5f : 1.0f);
        editor->encoder4.setValue(playbackValue);
        editor->paramDisplay4.setValue(playbackValue);
        if (editor->playbackMode == 0) {
            editor->paramDisplay4.setValueText("Stacked");
        } else if (editor->playbackMode == 1) {
            editor->paramDisplay4.setValueText("Round Robin");
        } else {
            editor->paramDisplay4.setValueText("Orbit");
        }
        
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
    } else {
        // Normal mode: Restore all normal mode parameter display values
        // Encoder 1: Repitch (-24 to +24 semitones)
        float pitchValue = (editor->repitchSemitones + 24.0f) / 48.0f;
        editor->encoder1.setValue(pitchValue);
        editor->paramDisplay1.setValue(pitchValue);
        int semitones = static_cast<int>(std::round(editor->repitchSemitones));
        juce::String pitchText = (semitones >= 0 ? "+" : "") + juce::String(semitones);
        editor->paramDisplay1.setValueText(pitchText);
        
        // Encoder 2: Start point (0 to sampleLength)
        float startValue = 0.0f;
        if (editor->sampleLength > 0) {
            startValue = static_cast<float>(editor->startPoint) / static_cast<float>(editor->sampleLength);
        }
        editor->encoder2.setValue(startValue);
        editor->paramDisplay2.setValue(startValue);
        double startTimeSeconds = static_cast<double>(editor->startPoint) / (editor->sampleRate > 0.0 ? editor->sampleRate : 44100.0);
        if (startTimeSeconds >= 1.0) {
            editor->paramDisplay2.setValueText(juce::String(startTimeSeconds, 2) + "s");
        } else {
            editor->paramDisplay2.setValueText(juce::String(static_cast<int>(startTimeSeconds * 1000.0)) + "ms");
        }
        
        // Encoder 3: End point (0 to sampleLength)
        float endValue = 0.0f;
        if (editor->sampleLength > 0) {
            endValue = static_cast<float>(editor->endPoint) / static_cast<float>(editor->sampleLength);
        }
        editor->encoder3.setValue(endValue);
        editor->paramDisplay3.setValue(endValue);
        double endTimeSeconds = static_cast<double>(editor->endPoint) / (editor->sampleRate > 0.0 ? editor->sampleRate : 44100.0);
        if (endTimeSeconds >= 1.0) {
            editor->paramDisplay3.setValueText(juce::String(endTimeSeconds, 2) + "s");
        } else {
            editor->paramDisplay3.setValueText(juce::String(static_cast<int>(endTimeSeconds * 1000.0)) + "ms");
        }
        
        // Encoder 4: Sample gain (0.0 to 2.0)
        float gainValue = editor->sampleGain / 2.0f;
        editor->encoder4.setValue(gainValue);
        editor->paramDisplay4.setValue(gainValue);
        editor->paramDisplay4.setValueText(juce::String(editor->sampleGain, 2) + "x");
        
        // Encoder 5: Attack (0-10000ms)
        float attackValue = editor->adsrAttackMs / 10000.0f;
        editor->encoder5.setValue(attackValue);
        editor->paramDisplay5.setValue(attackValue);
        if (editor->adsrAttackMs >= 1000.0f) {
            editor->paramDisplay5.setValueText(juce::String(editor->adsrAttackMs / 1000.0f, 2) + "s");
        } else {
            editor->paramDisplay5.setValueText(juce::String(static_cast<int>(editor->adsrAttackMs)) + "ms");
        }
        
        // Encoder 6: Decay (0-20000ms)
        float decayValue = editor->adsrDecayMs / 20000.0f;
        editor->encoder6.setValue(decayValue);
        editor->paramDisplay6.setValue(decayValue);
        if (editor->adsrDecayMs >= 1000.0f) {
            editor->paramDisplay6.setValueText(juce::String(editor->adsrDecayMs / 1000.0f, 2) + "s");
        } else {
            editor->paramDisplay6.setValueText(juce::String(static_cast<int>(editor->adsrDecayMs)) + "ms");
        }
        
        // Encoder 7: Sustain (0.0-1.0)
        editor->encoder7.setValue(editor->adsrSustain);
        editor->paramDisplay7.setValue(editor->adsrSustain);
        int sustainPercent = static_cast<int>(editor->adsrSustain * 100.0f);
        editor->paramDisplay7.setValueText(juce::String(sustainPercent) + "%");
        
        // Encoder 8: Release (0-20000ms)
        float releaseValue = editor->adsrReleaseMs / 20000.0f;
        editor->encoder8.setValue(releaseValue);
        editor->paramDisplay8.setValue(releaseValue);
        if (editor->adsrReleaseMs >= 1000.0f) {
            editor->paramDisplay8.setValueText(juce::String(editor->adsrReleaseMs / 1000.0f, 2) + "s");
        } else {
            editor->paramDisplay8.setValueText(juce::String(static_cast<int>(editor->adsrReleaseMs)) + "ms");
        }
        
        // Update ADSR visualization
        editor->updateADSR();
    }
}

