#include "EncoderSetupManager.h"
#include "PluginEditor.h"
#include <juce_core/juce_core.h>

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
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 1 does nothing (for now)
        } else {
            // Encoder 1: Repitch (-24 to +24 semitones)
            ed->repitchSemitones = (value - 0.5f) * 48.0f; // Map 0-1 to -24 to +24
            ed->updateSampleEditing();
            // Update parameter display 1 (Pitch) - normalize from -24/+24 to 0-1
            float normalizedPitch = (ed->repitchSemitones + 24.0f) / 48.0f;
            ed->paramDisplay1.setValue(normalizedPitch);
            // Format value: show semitones with +/- sign
            int semitones = static_cast<int>(std::round(ed->repitchSemitones));
            juce::String pitchText = (semitones >= 0 ? "+" : "") + juce::String(semitones);
            ed->paramDisplay1.setValueText(pitchText);
        }
    };
    ed->encoder1.onButtonPressed = [ed]() {
        // Encoder 1 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 1 does nothing (for now)
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
            // Shift mode: Encoder 2 does nothing (for now)
        } else {
            // Encoder 2: Start point (0 to sampleLength)
            if (ed->sampleLength > 0) {
                ed->startPoint = static_cast<int>(value * static_cast<float>(ed->sampleLength));
                // Ensure start point is before end point
                if (ed->startPoint >= ed->endPoint) {
                    ed->startPoint = std::max(0, ed->endPoint - 1);
                }
                ed->updateSampleEditing();
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
    };
    ed->encoder2.onButtonPressed = [ed]() {
        // Encoder 2 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 2 does nothing (for now)
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
            // Shift mode: Encoder 3 does nothing (for now)
        } else {
            // Encoder 3: End point (0 to sampleLength)
            if (ed->sampleLength > 0) {
                ed->endPoint = static_cast<int>(value * static_cast<float>(ed->sampleLength));
                // Ensure end point is after start point
                if (ed->endPoint <= ed->startPoint) {
                    ed->endPoint = std::min(ed->sampleLength, ed->startPoint + 1);
                }
                ed->updateSampleEditing();
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
    };
    ed->encoder3.onButtonPressed = [ed]() {
        // Encoder 3 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 3 does nothing (for now)
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
            // Shift mode: Encoder 4 does nothing (for now)
        } else {
            // Encoder 4: Sample gain (0.0 to 2.0)
            ed->sampleGain = value * 2.0f;
            ed->updateSampleEditing();
            ed->updateWaveformVisualization();
            // Update parameter display 4 (Gain) - normalize from 0-2.0 to 0-1
            ed->paramDisplay4.setValue(value);
            // Format value: show as multiplier (e.g., "1.5x")
            ed->paramDisplay4.setValueText(juce::String(ed->sampleGain, 2) + "x");
        }
    };
    ed->encoder4.onButtonPressed = [ed]() {
        // Encoder 4 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 4 does nothing (for now)
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
            // Shift mode: Encoder 5 does nothing (for now)
        } else {
            // Encoder 5: Attack (0-10000ms = 0-10 seconds)
            ed->adsrAttackMs = value * 10000.0f;
            ed->updateADSR();
            // Update parameter display 5 (Attack)
            ed->paramDisplay5.setValue(value);
            // Format value: show in ms, or seconds if >= 1000ms
            if (ed->adsrAttackMs >= 1000.0f) {
                ed->paramDisplay5.setValueText(juce::String(ed->adsrAttackMs / 1000.0f, 2) + "s");
            } else {
                ed->paramDisplay5.setValueText(juce::String(static_cast<int>(ed->adsrAttackMs)) + "ms");
            }
            // Show ADSR visualization when value changes (only if not already showing)
            if (!ed->isADSRDragging && !ed->isResettingADSR) {
                ed->isADSRDragging = true;
                // Add to component tree if not already added
                if (ed->adsrVisualization.getParentComponent() == nullptr) {
                    ed->addAndMakeVisible(&ed->adsrVisualization);
                }
                // Make visible first, then update
                ed->adsrVisualization.setAlpha(1.0f);
                ed->adsrVisualization.setVisible(true);
                ed->updateADSR();  // Ensure values are up to date
                ed->repaint();
            }
            // Don't reset fade-out timer here - let it continue if already fading
            // The fade-out will be properly set on drag end
        }
    };
    ed->encoder5.onDragStart = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            ed->isADSRDragging = true;
            // Add to component tree if not already added
            if (ed->adsrVisualization.getParentComponent() == nullptr) {
                ed->addAndMakeVisible(&ed->adsrVisualization);
            }
            // Make visible first, then update
            ed->adsrVisualization.setAlpha(1.0f);
            ed->adsrVisualization.setVisible(true);
            ed->updateADSR();  // Update with current values
            ed->repaint();  // Repaint the entire editor to ensure visualization shows
        }
    };
    ed->encoder5.onDragEnd = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = juce::Time::currentTimeMillis();
        }
    };
    ed->encoder5.onButtonPressed = [ed]() {
        // Encoder 5 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 5 does nothing (for now)
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // Hide ADSR visualization when resetting
            ed->adsrVisualization.setAlpha(0.0f);
            ed->adsrVisualization.setVisible(false);
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = 0;
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
            // Shift mode: Encoder 6 does nothing (for now)
        } else {
            // Encoder 6: Decay (0-20000ms = 0-20 seconds)
            ed->adsrDecayMs = value * 20000.0f;
            ed->updateADSR();
            // Update parameter display 6 (Decay)
            ed->paramDisplay6.setValue(value);
            // Format value: show in ms, or seconds if >= 1000ms
            if (ed->adsrDecayMs >= 1000.0f) {
                ed->paramDisplay6.setValueText(juce::String(ed->adsrDecayMs / 1000.0f, 2) + "s");
            } else {
                ed->paramDisplay6.setValueText(juce::String(static_cast<int>(ed->adsrDecayMs)) + "ms");
            }
            // Show ADSR visualization when value changes (only if not already showing)
            if (!ed->isADSRDragging && !ed->isResettingADSR) {
                ed->isADSRDragging = true;
                // Add to component tree if not already added
                if (ed->adsrVisualization.getParentComponent() == nullptr) {
                    ed->addAndMakeVisible(&ed->adsrVisualization);
                }
                // Make visible first, then update
                ed->adsrVisualization.setAlpha(1.0f);
                ed->adsrVisualization.setVisible(true);
                ed->updateADSR();  // Ensure values are up to date
                ed->repaint();
            }
            // Don't reset fade-out timer here - let it continue if already fading
            // The fade-out will be properly set on drag end
        }
    };
    ed->encoder6.onDragStart = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            ed->isADSRDragging = true;
            // Add to component tree if not already added
            if (ed->adsrVisualization.getParentComponent() == nullptr) {
                ed->addAndMakeVisible(&ed->adsrVisualization);
            }
            // Make visible first, then update
            ed->adsrVisualization.setAlpha(1.0f);
            ed->adsrVisualization.setVisible(true);
            ed->updateADSR();  // Update with current values
            ed->repaint();  // Repaint the entire editor to ensure visualization shows
        }
    };
    ed->encoder6.onDragEnd = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = juce::Time::currentTimeMillis();
        }
    };
    ed->encoder6.onButtonPressed = [ed]() {
        // Encoder 6 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 6 does nothing (for now)
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // Hide ADSR visualization when resetting
            ed->adsrVisualization.setAlpha(0.0f);
            ed->adsrVisualization.setVisible(false);
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = 0;
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
            // Shift mode: Encoder 7 does nothing (for now)
        } else {
            // Encoder 7: Sustain (0.0-1.0)
            ed->adsrSustain = value;
            ed->updateADSR();
            // Update parameter display 7 (Sustain)
            ed->paramDisplay7.setValue(value);
            // Format value: show as percentage
            int sustainPercent = static_cast<int>(ed->adsrSustain * 100.0f);
            ed->paramDisplay7.setValueText(juce::String(sustainPercent) + "%");
            // Show ADSR visualization when value changes (only if not already showing)
            if (!ed->isADSRDragging && !ed->isResettingADSR) {
                ed->isADSRDragging = true;
                // Add to component tree if not already added
                if (ed->adsrVisualization.getParentComponent() == nullptr) {
                    ed->addAndMakeVisible(&ed->adsrVisualization);
                }
                // Make visible first, then update
                ed->adsrVisualization.setAlpha(1.0f);
                ed->adsrVisualization.setVisible(true);
                ed->updateADSR();  // Ensure values are up to date
                ed->repaint();
            }
            // Don't reset fade-out timer here - let it continue if already fading
            // The fade-out will be properly set on drag end
        }
    };
    ed->encoder7.onDragStart = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            ed->isADSRDragging = true;
            // Add to component tree if not already added
            if (ed->adsrVisualization.getParentComponent() == nullptr) {
                ed->addAndMakeVisible(&ed->adsrVisualization);
            }
            // Make visible first, then update
            ed->adsrVisualization.setAlpha(1.0f);
            ed->adsrVisualization.setVisible(true);
            ed->updateADSR();  // Update with current values
            ed->repaint();  // Repaint the entire editor to ensure visualization shows
        }
    };
    ed->encoder7.onDragEnd = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = juce::Time::currentTimeMillis();
        }
    };
    ed->encoder7.onButtonPressed = [ed]() {
        // Encoder 7 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 7 does nothing (for now)
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // Hide ADSR visualization when resetting
            ed->adsrVisualization.setAlpha(0.0f);
            ed->adsrVisualization.setVisible(false);
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = 0;
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
            // Shift mode: Encoder 8 does nothing (for now)
        } else {
            // Encoder 8: Release (0-20000ms = 0-20 seconds)
            ed->adsrReleaseMs = value * 20000.0f;
            ed->updateADSR();
            // Update parameter display 8 (Release)
            ed->paramDisplay8.setValue(value);
            // Format value: show in ms, or seconds if >= 1000ms
            if (ed->adsrReleaseMs >= 1000.0f) {
                ed->paramDisplay8.setValueText(juce::String(ed->adsrReleaseMs / 1000.0f, 2) + "s");
            } else {
                ed->paramDisplay8.setValueText(juce::String(static_cast<int>(ed->adsrReleaseMs)) + "ms");
            }
            // Show ADSR visualization when value changes (only if not already showing)
            if (!ed->isADSRDragging && !ed->isResettingADSR) {
                ed->isADSRDragging = true;
                // Add to component tree if not already added
                if (ed->adsrVisualization.getParentComponent() == nullptr) {
                    ed->addAndMakeVisible(&ed->adsrVisualization);
                }
                // Make visible first, then update
                ed->adsrVisualization.setAlpha(1.0f);
                ed->adsrVisualization.setVisible(true);
                ed->updateADSR();  // Ensure values are up to date
                ed->repaint();
            }
            // Don't reset fade-out timer here - let it continue if already fading
            // The fade-out will be properly set on drag end
        }
    };
    ed->encoder8.onDragStart = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            ed->isADSRDragging = true;
            // Add to component tree if not already added
            if (ed->adsrVisualization.getParentComponent() == nullptr) {
                ed->addAndMakeVisible(&ed->adsrVisualization);
            }
            // Make visible first, then update
            ed->adsrVisualization.setAlpha(1.0f);
            ed->adsrVisualization.setVisible(true);
            ed->updateADSR();  // Update with current values
            ed->repaint();  // Repaint the entire editor to ensure visualization shows
        }
    };
    ed->encoder8.onDragEnd = [ed]() {
        if (!ed->shiftToggleButton.getToggleState()) {
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = juce::Time::currentTimeMillis();
        }
    };
    ed->encoder8.onButtonPressed = [ed]() {
        // Encoder 8 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 8 does nothing (for now)
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // Hide ADSR visualization when resetting
            ed->adsrVisualization.setAlpha(0.0f);
            ed->adsrVisualization.setVisible(false);
            ed->isADSRDragging = false;
            ed->adsrFadeOutStartTime = 0;
            // Normal mode: Release default = 20.0ms (value = 0.001)
            ed->encoder8.setValue(0.001f);
            ed->encoder8.onValueChanged(0.001f);
            // Clear reset flag after value change
            ed->isResettingADSR = false;
        }
    };
}

