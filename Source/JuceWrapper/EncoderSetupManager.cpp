#include "EncoderSetupManager.h"
#include "PluginEditor.h"
#include <juce_core/juce_core.h>
#include <cmath>

EncoderSetupManager::EncoderSetupManager(Op1CloneAudioProcessorEditor* editor)
    : editor(editor)
{
}

void EncoderSetupManager::setupEncoders() {
    setupEncoder1();
    setupEncoder2();
    setupEncoder3();
    setupEncoder4();
    setupEncoder5();
    setupEncoder6();
    setupEncoder7();
    setupEncoder8();
}

void EncoderSetupManager::setupEncoder1() {
    Op1CloneAudioProcessorEditor* ed = editor;  // Capture editor pointer, not 'this'
    ed->encoder1.onValueChanged = [ed](float value) {
        // Menu navigation is now handled by the menu encoder on the screen component
        // Encoder 1 is now only for normal operation (pitch/filter cutoff)
        
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 1 = LP Filter Cutoff (20-20000 Hz)
            // Map 0-1 to 20-20000 Hz (logarithmic)
            float normalized = value;
            ed->lpCutoffHz = 20.0f * std::pow(1000.0f, normalized); // Logarithmic mapping
            ed->lpCutoffHz = std::max(20.0f, std::min(20000.0f, ed->lpCutoffHz));
            // Send to processor
            ed->audioProcessor.setLPFilterCutoff(ed->lpCutoffHz);
            // Update parameter display
            ed->paramDisplay1.setValue(value);
            if (ed->lpCutoffHz >= 1000.0f) {
                ed->paramDisplay1.setValueText(juce::String(ed->lpCutoffHz / 1000.0f, 1) + "kHz");
            } else {
                ed->paramDisplay1.setValueText(juce::String(static_cast<int>(ed->lpCutoffHz)) + "Hz");
            }
        } else {
            // Encoder 1: Repitch (-24 to +24 semitones)
            ed->repitchSemitones = (value - 0.5f) * 48.0f; // Map 0-1 to -24 to +24
            // Update current slot's parameter in adapter (this only affects new notes, not existing voices)
            ed->audioProcessor.setSlotRepitch(ed->currentSlotIndex, ed->repitchSemitones);
            // Update parameter display 1 (Pitch) - normalize from -24/+24 to 0-1
            float normalizedPitch = (ed->repitchSemitones + 24.0f) / 48.0f;
            ed->paramDisplay1.setValue(normalizedPitch);
            // Format value: show semitones with +/- sign
            int semitones = static_cast<int>(std::round(ed->repitchSemitones));
            juce::String pitchText = (semitones >= 0 ? "+" : "") + juce::String(semitones);
            ed->paramDisplay1.setValueText(pitchText);
        }
        
        // Auto-save to current slot when parameter changes
        ed->saveCurrentStateToSlot(ed->currentSlotIndex);
    };
    ed->encoder1.onButtonPressed = [ed]() {
        // Check if instrument menu is open - if so, select instrument
        if (ed->instrumentMenuOpen) {
            ed->screenComponent.selectInstrument();
            ed->repaint();
            return;  // Don't process normal button behavior when menu is open
        }
        
        // Encoder 1 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Cutoff default = 20kHz (fully open, value = 1.0)
            ed->lpCutoffHz = 20000.0f;
            ed->audioProcessor.setLPFilterCutoff(20000.0f);
            ed->encoder1.setValue(1.0f);
            ed->encoder1.onValueChanged(1.0f);
        } else {
            // Normal mode: Repitch default = 0 semitones (value = 0.5)
            ed->encoder1.setValue(0.5f);
            ed->encoder1.onValueChanged(0.5f);
        }
    };
}

void EncoderSetupManager::setupEncoder2() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder2.onValueChanged = [ed](float value) {
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 2 = LP Filter Resonance (0.0-4.0)
            ed->lpResonance = value * 4.0f; // Map 0-1 to 0.0-4.0
            // Send to processor
            ed->audioProcessor.setLPFilterResonance(ed->lpResonance);
            // Update parameter display
            ed->paramDisplay2.setValue(value);
            ed->paramDisplay2.setValueText(juce::String(ed->lpResonance, 2));
        } else {
            // Encoder 2: Start point (0 to sampleLength)
            if (ed->sampleLength > 0) {
                ed->startPoint = static_cast<int>(value * static_cast<float>(ed->sampleLength));
                // Ensure start point is before end point
                if (ed->startPoint >= ed->endPoint) {
                    ed->startPoint = std::max(0, ed->endPoint - 1);
                }
                // Update current slot's parameter in adapter (this only affects new notes, not existing voices)
                ed->audioProcessor.setSlotStartPoint(ed->currentSlotIndex, ed->startPoint);
                ed->updateWaveformVisualization();
                // Force repaint
                ed->screenComponent.repaint();
                // Update parameter display 2 (Start)
                ed->paramDisplay2.setValue(value);
                // Format value: show as time position (e.g., "1.5s" or "500ms")
                double startTimeSeconds = static_cast<double>(ed->startPoint) / ed->sampleRate;
                if (startTimeSeconds >= 1.0) {
                    ed->paramDisplay2.setValueText(juce::String(startTimeSeconds, 2) + "s");
                } else {
                    ed->paramDisplay2.setValueText(juce::String(static_cast<int>(startTimeSeconds * 1000.0)) + "ms");
                }
            }
        }
        
        // Auto-save to current slot when parameter changes
        ed->saveCurrentStateToSlot(ed->currentSlotIndex);
    };
    ed->encoder2.onButtonPressed = [ed]() {
        // Encoder 2 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Resonance default = 1.0
            float defaultValue = (1.0f - 0.1f) / 9.9f;
            ed->encoder2.setValue(defaultValue);
            ed->encoder2.onValueChanged(defaultValue);
        } else {
            // Normal mode: Start point default = 0 (value = 0.0)
            ed->encoder2.setValue(0.0f);
            ed->encoder2.onValueChanged(0.0f);
        }
    };
}

void EncoderSetupManager::setupEncoder3() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder3.onValueChanged = [ed](float value) {
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 3 = Drive (0.0 to 24.0 dB)
            ed->lpDriveDb = value * 24.0f;  // Map 0-1 to 0-24 dB
            ed->audioProcessor.setLPFilterDrive(ed->lpDriveDb);
            // Update parameter display 3 (Drive)
            ed->paramDisplay3.setValue(value);
            ed->paramDisplay3.setValueText(juce::String(ed->lpDriveDb, 1) + " dB");
        } else {
            // Encoder 3: End point (0 to sampleLength)
            if (ed->sampleLength > 0) {
                ed->endPoint = static_cast<int>(value * static_cast<float>(ed->sampleLength));
                // Ensure end point is after start point
                if (ed->endPoint <= ed->startPoint) {
                    ed->endPoint = std::min(ed->sampleLength, ed->startPoint + 1);
                }
                // Update current slot's parameter in adapter (this only affects new notes, not existing voices)
                ed->audioProcessor.setSlotEndPoint(ed->currentSlotIndex, ed->endPoint);
                ed->updateWaveformVisualization();
                // Force repaint
                ed->screenComponent.repaint();
                // Update parameter display 3 (End)
                ed->paramDisplay3.setValue(value);
                // Format value: show as time position (e.g., "1.5s" or "500ms")
                double endTimeSeconds = static_cast<double>(ed->endPoint) / ed->sampleRate;
                if (endTimeSeconds >= 1.0) {
                    ed->paramDisplay3.setValueText(juce::String(endTimeSeconds, 2) + "s");
                } else {
                    ed->paramDisplay3.setValueText(juce::String(static_cast<int>(endTimeSeconds * 1000.0)) + "ms");
                }
            }
        }
        
        // Auto-save to current slot when parameter changes
        ed->saveCurrentStateToSlot(ed->currentSlotIndex);
    };
    ed->encoder3.onButtonPressed = [ed]() {
        // Encoder 3 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Drive default = 0.0 dB (value = 0.0)
            ed->lpDriveDb = 0.0f;
            ed->audioProcessor.setLPFilterDrive(ed->lpDriveDb);
            ed->encoder3.setValue(0.0f);
            ed->encoder3.onValueChanged(0.0f);
        } else {
            // Normal mode: End point default = full length (value = 1.0)
            ed->encoder3.setValue(1.0f);
            ed->encoder3.onValueChanged(1.0f);
        }
    };
}

void EncoderSetupManager::setupEncoder4() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder4.onValueChanged = [ed](float value) {
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 4 = Playback mode (Stacked/Round Robin)
            ed->playbackMode = value >= 0.5f ? 1 : 0;  // 0 = Stacked, 1 = Round Robin
            ed->audioProcessor.setPlaybackMode(ed->playbackMode);
            // Update parameter display
            ed->paramDisplay4.setValue(value);
            ed->paramDisplay4.setValueText(ed->playbackMode == 0 ? "Stacked" : "Round Robin");
        } else {
            // Encoder 4: Sample gain (0.0 to 2.0)
            ed->sampleGain = value * 2.0f;
            // Update current slot's parameter in adapter (this only affects new notes, not existing voices)
            ed->audioProcessor.setSlotSampleGain(ed->currentSlotIndex, ed->sampleGain);
            ed->updateWaveformVisualization();
            // Update parameter display 4 (Gain) - normalize from 0-2.0 to 0-1
            ed->paramDisplay4.setValue(value);
            // Format value: show as multiplier (e.g., "1.5x")
            ed->paramDisplay4.setValueText(juce::String(ed->sampleGain, 2) + "x");
        }
        
        // Auto-save to current slot when parameter changes
        ed->saveCurrentStateToSlot(ed->currentSlotIndex);
    };
    ed->encoder4.onButtonPressed = [ed]() {
        // Encoder 4 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 4 - no function (speed knob removed)
            // Do nothing in shift mode
        } else {
            // Normal mode: Sample gain default = 1.0x (value = 0.5)
            ed->encoder4.setValue(0.5f);
            ed->encoder4.onValueChanged(0.5f);
        }
    };
}

void EncoderSetupManager::setupEncoder5() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder5.onValueChanged = [ed](float value) {
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 5 = Loop Start Point (0 to sampleLength)
            // Allow loop start to be past loop end for reverse playback
            if (ed->sampleLength > 0) {
                ed->loopStartPoint = static_cast<int>(value * static_cast<float>(ed->sampleLength));
                // No constraint - allow loop start to be past loop end for reverse playback
                // Send to processor
                ed->audioProcessor.setLoopPoints(ed->loopStartPoint, ed->loopEndPoint);
                // Update waveform visualization to show loop markers
                ed->updateWaveformVisualization();
                // Update parameter display
                ed->paramDisplay5.setValue(value);
                double loopStartTimeSeconds = static_cast<double>(ed->loopStartPoint) / ed->sampleRate;
                if (loopStartTimeSeconds >= 1.0) {
                    ed->paramDisplay5.setValueText(juce::String(loopStartTimeSeconds, 2) + "s");
                } else {
                    ed->paramDisplay5.setValueText(juce::String(static_cast<int>(loopStartTimeSeconds * 1000.0)) + "ms");
                }
            }
        } else {
            // Encoder 5: Attack (0-10000ms = 0-10 seconds)
            ed->adsrAttackMs = value * 10000.0f;
            // Update current slot's ADSR parameters in adapter (this only affects new notes, not existing voices)
            ed->audioProcessor.setSlotADSR(ed->currentSlotIndex, ed->adsrAttackMs, ed->adsrDecayMs, ed->adsrSustain, ed->adsrReleaseMs);
            // Update visualization only (not engine parameters - those are per-slot now)
            ed->adsrPillComponent.setADSR(ed->adsrAttackMs, ed->adsrDecayMs, ed->adsrSustain, ed->adsrReleaseMs);
            // Update parameter display 5 (Attack)
            ed->paramDisplay5.setValue(value);
            // Format value: show in ms, or seconds if >= 1000ms
            if (ed->adsrAttackMs >= 1000.0f) {
                ed->paramDisplay5.setValueText(juce::String(ed->adsrAttackMs / 1000.0f, 2) + "s");
            } else {
                ed->paramDisplay5.setValueText(juce::String(static_cast<int>(ed->adsrAttackMs)) + "ms");
            }
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
            /*
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
            /*
            // Show ADSR visualization when value changes (only if not already showing)
            if (!ed->isADSRDragging && !ed->isResettingADSR) {
                ed->isADSRDragging = true;
                // Add to component tree if not already added
                if (ed->adsrVisualization.getParentComponent() == nullptr) {
                    ed->adsrVisualization.setPaintingEnabled(true);  // CRITICAL: Enable painting BEFORE setting bounds
                    ed->addAndMakeVisible(&ed->adsrVisualization);
                    ed->adsrVisualization.setAlwaysOnTop(true);  // Set always on top when showing
                    ed->setADSRVisualizationBounds();  // Set bounds when adding to tree (now paintingEnabled is true)
                }
                // Make visible first, then update
                ed->adsrVisualization.setAlpha(1.0f);
                ed->adsrVisualization.setVisible(true);
                ed->updateADSR();  // Ensure values are up to date
                ed->repaint();
            }
            */
            // Don't reset fade-out timer here - let it continue if already fading
            // The fade-out will be properly set on drag end
        }
        
        // Auto-save to current slot when parameter changes
        ed->saveCurrentStateToSlot(ed->currentSlotIndex);
    };
    ed->encoder5.onDragStart = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
        }
    };
    ed->encoder5.onDragEnd = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            // OLD fade-out code (COMMENTED OUT - replaced with pill component)
            // Pill component stays visible, no fade-out needed
            /*
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = juce::Time::currentTimeMillis();
            */
        }
    };
    ed->encoder5.onButtonPressed = [ed]() {
        // Encoder 5 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Loop Start default = 0 (value = 0.0)
            ed->encoder5.setValue(0.0f);
            ed->encoder5.onValueChanged(0.0f);
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // OLD hide ADSR visualization code (COMMENTED OUT - replaced with pill component)
            // Pill component stays visible, just update it
            /*
            ed->adsrVisualization.setAlpha(0.0f);
            ed->adsrVisualization.setVisible(false);
            */
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = 0;
            ed->updateADSR();  // Update pill visualization with reset values
            // Normal mode: Attack default = 2.0ms (value = 0.0002)
            ed->encoder5.setValue(0.0002f);
            ed->encoder5.onValueChanged(0.0002f);
            // Clear reset flag after value change
            ed->isResettingADSR = false;
        }
    };
}

void EncoderSetupManager::setupEncoder6() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder6.onValueChanged = [ed](float value) {
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 6 = Loop End Point (0 to sampleLength)
            // Allow loop end to be before loop start for reverse playback
            if (ed->sampleLength > 0) {
                ed->loopEndPoint = static_cast<int>(value * static_cast<float>(ed->sampleLength));
                // No constraint - allow loop end to be before loop start for reverse playback
                // Send to processor
                ed->audioProcessor.setLoopPoints(ed->loopStartPoint, ed->loopEndPoint);
                // Update waveform visualization to show loop markers
                ed->updateWaveformVisualization();
                // Update parameter display
                ed->paramDisplay6.setValue(value);
                double loopEndTimeSeconds = static_cast<double>(ed->loopEndPoint) / ed->sampleRate;
                if (loopEndTimeSeconds >= 1.0) {
                    ed->paramDisplay6.setValueText(juce::String(loopEndTimeSeconds, 2) + "s");
                } else {
                    ed->paramDisplay6.setValueText(juce::String(static_cast<int>(loopEndTimeSeconds * 1000.0)) + "ms");
                }
            }
        } else {
            // Encoder 6: Decay (0-20000ms = 0-20 seconds)
            ed->adsrDecayMs = value * 20000.0f;
            // Update current slot's ADSR parameters in adapter (this only affects new notes, not existing voices)
            ed->audioProcessor.setSlotADSR(ed->currentSlotIndex, ed->adsrAttackMs, ed->adsrDecayMs, ed->adsrSustain, ed->adsrReleaseMs);
            // Update visualization only (not engine parameters - those are per-slot now)
            ed->adsrPillComponent.setADSR(ed->adsrAttackMs, ed->adsrDecayMs, ed->adsrSustain, ed->adsrReleaseMs);
            // Update parameter display 6 (Decay)
            ed->paramDisplay6.setValue(value);
            // Format value: show in ms, or seconds if >= 1000ms
            if (ed->adsrDecayMs >= 1000.0f) {
                ed->paramDisplay6.setValueText(juce::String(ed->adsrDecayMs / 1000.0f, 2) + "s");
            } else {
                ed->paramDisplay6.setValueText(juce::String(static_cast<int>(ed->adsrDecayMs)) + "ms");
            }
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
            /*
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
            /*
            // Show ADSR visualization when value changes (only if not already showing)
            if (!ed->isADSRDragging && !ed->isResettingADSR) {
                ed->isADSRDragging = true;
                // Add to component tree if not already added
                if (ed->adsrVisualization.getParentComponent() == nullptr) {
                    ed->adsrVisualization.setPaintingEnabled(true);  // CRITICAL: Enable painting BEFORE setting bounds
                    ed->addAndMakeVisible(&ed->adsrVisualization);
                    ed->adsrVisualization.setAlwaysOnTop(true);  // Set always on top when showing
                    ed->setADSRVisualizationBounds();  // Set bounds when adding to tree (now paintingEnabled is true)
                }
                // Make visible first, then update
                ed->adsrVisualization.setAlpha(1.0f);
                ed->adsrVisualization.setVisible(true);
                ed->updateADSR();  // Ensure values are up to date
                ed->repaint();
            }
            */
            // Don't reset fade-out timer here - let it continue if already fading
            // The fade-out will be properly set on drag end
        }
        
        // Auto-save to current slot when parameter changes
        ed->saveCurrentStateToSlot(ed->currentSlotIndex);
    };
    ed->encoder6.onDragStart = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
        }
    };
    ed->encoder6.onDragEnd = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            // OLD fade-out code (COMMENTED OUT - replaced with pill component)
            // Pill component stays visible, no fade-out needed
            /*
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = juce::Time::currentTimeMillis();
            */
        }
    };
    ed->encoder6.onButtonPressed = [ed]() {
        // Encoder 6 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Loop End default = full length (value = 1.0)
            ed->encoder6.setValue(1.0f);
            ed->encoder6.onValueChanged(1.0f);
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // OLD hide ADSR visualization code (COMMENTED OUT - replaced with pill component)
            // Pill component stays visible, just update it
            /*
            ed->adsrVisualization.setAlpha(0.0f);
            ed->adsrVisualization.setVisible(false);
            */
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = 0;
            ed->updateADSR();  // Update pill visualization with reset values
            // Normal mode: Decay default = 0.0ms (value = 0.0)
            ed->encoder6.setValue(0.0f);
            ed->encoder6.onValueChanged(0.0f);
            // Clear reset flag after value change
            ed->isResettingADSR = false;
        }
    };
}

void EncoderSetupManager::setupEncoder7() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder7.onValueChanged = [ed](float value) {
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 7 = Loop (on/off)
            ed->loopEnabled = value >= 0.5f;
            ed->audioProcessor.setLoopEnabled(ed->loopEnabled);
            ed->audioProcessor.setLoopPoints(ed->loopStartPoint, ed->loopEndPoint);
            // Update parameter display
            ed->paramDisplay7.setValue(value);
            ed->paramDisplay7.setValueText(ed->loopEnabled ? "ON" : "OFF");
            // Update waveform to show/hide loop markers
            ed->updateWaveformVisualization();
            // Update loop controls visibility/grey state
            ed->updateLoopControlsState();
        } else {
            // Encoder 7: Sustain (0.0-1.0)
            ed->adsrSustain = value;
            // Update current slot's ADSR parameters in adapter (this only affects new notes, not existing voices)
            ed->audioProcessor.setSlotADSR(ed->currentSlotIndex, ed->adsrAttackMs, ed->adsrDecayMs, ed->adsrSustain, ed->adsrReleaseMs);
            // Update visualization only (not engine parameters - those are per-slot now)
            ed->adsrPillComponent.setADSR(ed->adsrAttackMs, ed->adsrDecayMs, ed->adsrSustain, ed->adsrReleaseMs);
            // Update parameter display 7 (Sustain)
            ed->paramDisplay7.setValue(value);
            // Format value: show as percentage
            int sustainPercent = static_cast<int>(ed->adsrSustain * 100.0f);
            ed->paramDisplay7.setValueText(juce::String(sustainPercent) + "%");
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
            /*
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
            /*
            // Show ADSR visualization when value changes (only if not already showing)
            if (!ed->isADSRDragging && !ed->isResettingADSR) {
                ed->isADSRDragging = true;
                // Add to component tree if not already added
                if (ed->adsrVisualization.getParentComponent() == nullptr) {
                    ed->adsrVisualization.setPaintingEnabled(true);  // CRITICAL: Enable painting BEFORE setting bounds
                    ed->addAndMakeVisible(&ed->adsrVisualization);
                    ed->adsrVisualization.setAlwaysOnTop(true);  // Set always on top when showing
                    ed->setADSRVisualizationBounds();  // Set bounds when adding to tree (now paintingEnabled is true)
                }
                // Make visible first, then update
                ed->adsrVisualization.setAlpha(1.0f);
                ed->adsrVisualization.setVisible(true);
                ed->updateADSR();  // Ensure values are up to date
                ed->repaint();
            }
            */
            // Don't reset fade-out timer here - let it continue if already fading
            // The fade-out will be properly set on drag end
        }
        
        // Auto-save to current slot when parameter changes
        ed->saveCurrentStateToSlot(ed->currentSlotIndex);
    };
    ed->encoder7.onDragStart = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
        }
    };
    ed->encoder7.onDragEnd = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            // OLD fade-out code (COMMENTED OUT - replaced with pill component)
            // Pill component stays visible, no fade-out needed
            /*
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = juce::Time::currentTimeMillis();
            */
        }
    };
    ed->encoder7.onButtonPressed = [ed]() {
        // Encoder 7 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Loop default = OFF (value = 0.0)
            ed->loopEnabled = false;
            ed->audioProcessor.setLoopEnabled(false);
            ed->encoder7.setValue(0.0f);
            ed->encoder7.onValueChanged(0.0f);
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // OLD hide ADSR visualization code (COMMENTED OUT - replaced with pill component)
            // Pill component stays visible, just update it
            /*
            ed->adsrVisualization.setAlpha(0.0f);
            ed->adsrVisualization.setVisible(false);
            */
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = 0;
            ed->updateADSR();  // Update pill visualization with reset values
            // Normal mode: Sustain default = 1.0 (value = 1.0)
            ed->encoder7.setValue(1.0f);
            ed->encoder7.onValueChanged(1.0f);
            // Clear reset flag after value change
            ed->isResettingADSR = false;
        }
    };
}

void EncoderSetupManager::setupEncoder8() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder8.onValueChanged = [ed](float value) {
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 8 = Playback (Mono/Poly toggle)
            // Value < 0.5 = Mono, >= 0.5 = Poly
            bool isPoly = value >= 0.5f;
            ed->isPolyphonic = isPoly;
            ed->audioProcessor.setPlaybackMode(isPoly);
            // Update parameter display
            ed->paramDisplay8.setValue(value);
            ed->paramDisplay8.setValueText(isPoly ? "Poly" : "Mono");
        } else {
            // Encoder 8: Release (0-20000ms = 0-20 seconds)
            ed->adsrReleaseMs = value * 20000.0f;
            // Update current slot's ADSR parameters in adapter (this only affects new notes, not existing voices)
            ed->audioProcessor.setSlotADSR(ed->currentSlotIndex, ed->adsrAttackMs, ed->adsrDecayMs, ed->adsrSustain, ed->adsrReleaseMs);
            // Update visualization only (not engine parameters - those are per-slot now)
            ed->adsrPillComponent.setADSR(ed->adsrAttackMs, ed->adsrDecayMs, ed->adsrSustain, ed->adsrReleaseMs);
            // Update parameter display 8 (Release)
            ed->paramDisplay8.setValue(value);
            // Format value: show in ms, or seconds if >= 1000ms
            if (ed->adsrReleaseMs >= 1000.0f) {
                ed->paramDisplay8.setValueText(juce::String(ed->adsrReleaseMs / 1000.0f, 2) + "s");
            } else {
                ed->paramDisplay8.setValueText(juce::String(static_cast<int>(ed->adsrReleaseMs)) + "ms");
            }
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
        }
        
        // Auto-save to current slot when parameter changes
        ed->saveCurrentStateToSlot(ed->currentSlotIndex);
    };
    ed->encoder8.onDragStart = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
        }
    };
    ed->encoder8.onDragEnd = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            // OLD fade-out code (COMMENTED OUT - replaced with pill component)
            // Pill component stays visible, no fade-out needed
            /*
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = juce::Time::currentTimeMillis();
            */
        }
    };
    ed->encoder8.onButtonPressed = [ed]() {
        // Encoder 8 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Playback default = Poly (value = 1.0)
            ed->isPolyphonic = true;
            ed->audioProcessor.setPlaybackMode(true);
            ed->encoder8.setValue(1.0f);
            ed->encoder8.onValueChanged(1.0f);
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // OLD hide ADSR visualization code (COMMENTED OUT - replaced with pill component)
            // Pill component stays visible, just update it
            /*
            ed->adsrVisualization.setAlpha(0.0f);
            ed->adsrVisualization.setVisible(false);
            */
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = 0;
            ed->updateADSR();  // Update pill visualization with reset values
            // Normal mode: Release default = 20.0ms (value = 0.001)
            ed->encoder8.setValue(0.001f);
            ed->encoder8.onValueChanged(0.001f);
            // Clear reset flag after value change
            ed->isResettingADSR = false;
        }
    };
}

