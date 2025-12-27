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
        // Check if orbit menu is open - if so, handle orbit-specific controls
        if (ed->playbackMode == 2 && ed->orbitMenuOpen) {
            // Encoder 1: Orbit Rate (musical note value)
            // Map 0-1 to note values: REVERSED - 0=4 bars (slowest), 8=1/64 (fastest)
            // Note values: 0=4 bars, 1=2 bars, 2=1 bar, 3=1/2, 4=1/4, 5=1/8, 6=1/16, 7=1/32, 8=1/64
            int numNoteValues = 9;  // 0-8
            // Reverse mapping: value 0.0 -> noteValue 8 (1/64), value 1.0 -> noteValue 0 (4 bars)
            float reversedValue = 1.0f - value;
            int noteValue = static_cast<int>(reversedValue * (numNoteValues - 1) + 0.5f);  // Round to nearest
            noteValue = std::max(0, std::min(8, noteValue));  // Clamp to 0-8
            ed->orbitRateNoteValue = noteValue;
            
            // Calculate rate in Hz from musical timing
            // Note values are now reversed: 0=4 bars (slowest), 8=1/64 (fastest)
            float bpm = static_cast<float>(ed->projectBPM);
            float baseDuration = 0.0f;
            // Fix: Rate calculation was 4x too fast. Calculate duration correctly.
            // At 120 BPM, one beat (1/4 note) = 60/120 = 0.5 seconds = 2 Hz
            // Previous calculation: 60/(120*4) = 0.125 seconds = 8 Hz (4x too fast)
            switch (noteValue) {
                case 0: baseDuration = 60.0f * 4.0f * 4.0f / bpm; break;     // 4 bars (4 beats per bar)
                case 1: baseDuration = 60.0f * 2.0f * 4.0f / bpm; break;     // 2 bars
                case 2: baseDuration = 60.0f * 4.0f / bpm; break;            // 1 bar (4 beats)
                case 3: baseDuration = 60.0f * 2.0f / bpm; break;            // 1/2 (half note = 2 beats)
                case 4: baseDuration = 60.0f / bpm; break;                   // 1/4 (quarter note = 1 beat)
                case 5: baseDuration = 60.0f / (bpm * 2.0f); break;          // 1/8 (eighth note = 0.5 beats)
                case 6: baseDuration = 60.0f / (bpm * 4.0f); break;          // 1/16
                case 7: baseDuration = 60.0f / (bpm * 8.0f); break;          // 1/32
                case 8: baseDuration = 60.0f / (bpm * 16.0f); break;         // 1/64 (fastest)
            }
            
            // Apply triplet/dotted modifiers
            if (ed->orbitRateTriplet) {
                baseDuration *= 2.0f / 3.0f;  // Triplet: 2/3 duration
            }
            if (ed->orbitRateDotted) {
                baseDuration *= 1.5f;  // Dotted: 1.5x duration
            }
            
            ed->orbitRateHz = 1.0f / baseDuration;  // Convert to Hz
            ed->audioProcessor.setOrbitRate(ed->orbitRateHz);
            
            // Update display
            juce::String rateText = EncoderSetupManager::getNoteValueString(noteValue);
            if (ed->orbitRateTriplet) rateText += "T";
            if (ed->orbitRateDotted) rateText += ".";
            // Display value is reversed: 0.0 shows 4 bars, 1.0 shows 1/64
            ed->paramDisplay1.setValue(reversedValue);  // Use reversed value for display bar
            ed->paramDisplay1.setValueText(rateText);
            return;
        }
        
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
            // Format value: show as semitones (e.g., "+12st", "-5st", "0st")
            int semitones = static_cast<int>(ed->repitchSemitones);
            if (semitones > 0) {
                ed->paramDisplay1.setValueText("+" + juce::String(semitones) + "st");
            } else if (semitones < 0) {
                ed->paramDisplay1.setValueText(juce::String(semitones) + "st");
            } else {
                ed->paramDisplay1.setValueText("0st");
            }
        }
        
        // Auto-save to current slot when parameter changes
        ed->saveCurrentStateToSlot(ed->currentSlotIndex);
    };
    ed->encoder1.onButtonPressed = [ed]() {
        // Encoder 1 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Filter cutoff default = 20000 Hz (value = 1.0)
            ed->encoder1.setValue(1.0f);
            ed->encoder1.onValueChanged(1.0f);
        } else {
            // Normal mode: Pitch default = 0 semitones (value = 0.5)
            ed->encoder1.setValue(0.5f);
            ed->encoder1.onValueChanged(0.5f);
        }
    };
}

void EncoderSetupManager::setupEncoder2() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder2.onValueChanged = [ed](float value) {
        // Check if orbit menu is open - if so, handle orbit-specific controls
        if (ed->playbackMode == 2 && ed->orbitMenuOpen) {
            // Encoder 2: Orbit Shape
            // 0=Circle, 1=PingPong, 2=Corners, 3=RandomSmooth, 4=Figure8, 5=ZigZag, 6=Spiral, 7=Square
            int numShapes = 8;
            int shape = static_cast<int>(value * (numShapes - 1));
            ed->orbitShape = shape;
            ed->audioProcessor.setOrbitShape(shape);
            
            // Update display
            juce::String shapeText;
            switch (shape) {
                case 0: shapeText = "Circle"; break;
                case 1: shapeText = "PingPong"; break;
                case 2: shapeText = "Corners"; break;
                case 3: shapeText = "Random"; break;
                case 4: shapeText = "Figure8"; break;
                case 5: shapeText = "ZigZag"; break;
                case 6: shapeText = "Spiral"; break;
                case 7: shapeText = "Square"; break;
                default: shapeText = "Circle"; break;
            }
            ed->paramDisplay2.setValue(value);
            ed->paramDisplay2.setValueText(shapeText);
            return;
        }
        
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 2 = LP Filter Resonance (0-100)
            float normalized = value;
            ed->lpResonance = normalized * 100.0f;
            ed->lpResonance = std::max(0.0f, std::min(100.0f, ed->lpResonance));
            // Send to processor
            ed->audioProcessor.setLPFilterResonance(ed->lpResonance);
            // Update parameter display
            ed->paramDisplay2.setValue(value);
            ed->paramDisplay2.setValueText(juce::String(static_cast<int>(ed->lpResonance)));
        } else {
            // Encoder 2: Start point (0 to sampleLength)
            std::vector<float> leftChannel, rightChannel;
            ed->audioProcessor.getSlotStereoSampleDataForVisualization(ed->currentSlotIndex, leftChannel, rightChannel);
            if (!leftChannel.empty()) {
                ed->sampleLength = static_cast<int>(leftChannel.size());
            }
            if (ed->sampleLength > 0) {
                ed->startPoint = static_cast<int>(value * static_cast<float>(ed->sampleLength));
                ed->startPoint = std::max(0, std::min(ed->sampleLength - 1, ed->startPoint));
                // Send to processor for current slot
                ed->audioProcessor.setSlotStartPoint(ed->currentSlotIndex, ed->startPoint);
                // Save state to current slot
                ed->saveCurrentStateToSlot(ed->currentSlotIndex);
                // Update waveform visualization to show start point marker
                ed->updateWaveformVisualization();
                // Update parameter display
                ed->paramDisplay2.setValue(value);
                // Update sampleRate from current slot
                ed->sampleRate = ed->audioProcessor.getSlotSourceSampleRate(ed->currentSlotIndex);
                double startTimeSeconds = static_cast<double>(ed->startPoint) / (ed->sampleRate > 0.0 ? ed->sampleRate : 44100.0);
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
            // Shift mode: Filter resonance default = 0 (value = 0.0)
            ed->encoder2.setValue(0.0f);
            ed->encoder2.onValueChanged(0.0f);
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
        // Check if orbit menu is open - if so, handle orbit-specific controls
        if (ed->playbackMode == 2 && ed->orbitMenuOpen) {
            // Encoder 3: Orbit Curve (0.0-1.0) - controls smoothing/easing of rate
            ed->orbitCurve = value;
            // Curve affects the smoothing coefficient in orbit blender
            // Higher curve = smoother transitions (we can apply this to the smoothing coeff)
            // For now, store it - we'll apply it to the orbit blender smoothing
            ed->paramDisplay3.setValue(value);
            int curvePercent = static_cast<int>(value * 100.0f);
            ed->paramDisplay3.setValueText(juce::String(curvePercent) + "%");
            return;
        }
        
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 3 = LP Filter Drive (0-100)
            float normalized = value;
            ed->lpDriveDb = normalized * 100.0f;
            ed->lpDriveDb = std::max(0.0f, std::min(100.0f, ed->lpDriveDb));
            // Send to processor
            ed->audioProcessor.setLPFilterDrive(ed->lpDriveDb);
            // Update parameter display
            ed->paramDisplay3.setValue(value);
            ed->paramDisplay3.setValueText(juce::String(static_cast<int>(ed->lpDriveDb)));
        } else {
            // Encoder 3: End point (0 to sampleLength)
            std::vector<float> leftChannel, rightChannel;
            ed->audioProcessor.getSlotStereoSampleDataForVisualization(ed->currentSlotIndex, leftChannel, rightChannel);
            if (!leftChannel.empty()) {
                ed->sampleLength = static_cast<int>(leftChannel.size());
            }
            if (ed->sampleLength > 0) {
                ed->endPoint = static_cast<int>(value * static_cast<float>(ed->sampleLength));
                ed->endPoint = std::max(0, std::min(ed->sampleLength - 1, ed->endPoint));
                // Send to processor for current slot
                ed->audioProcessor.setSlotEndPoint(ed->currentSlotIndex, ed->endPoint);
                // Save state to current slot
                ed->saveCurrentStateToSlot(ed->currentSlotIndex);
                // Update waveform visualization to show end point marker
                ed->updateWaveformVisualization();
                // Update parameter display
                ed->paramDisplay3.setValue(value);
                // Update sampleRate from current slot
                ed->sampleRate = ed->audioProcessor.getSlotSourceSampleRate(ed->currentSlotIndex);
                double endTimeSeconds = static_cast<double>(ed->endPoint) / (ed->sampleRate > 0.0 ? ed->sampleRate : 44100.0);
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
            // Shift mode: Filter drive default = 0 (value = 0.0)
            ed->encoder3.setValue(0.0f);
            ed->encoder3.onValueChanged(0.0f);
        } else {
            // Normal mode: End point default = sampleLength (value = 1.0)
            ed->encoder3.setValue(1.0f);
            ed->encoder3.onValueChanged(1.0f);
        }
    };
}

void EncoderSetupManager::setupEncoder4() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder4.onValueChanged = [ed](float value) {
        // Check if orbit menu is open - if so, do nothing (encoder 4 disabled in orbit mode)
        if (ed->playbackMode == 2 && ed->orbitMenuOpen) {
            // Encoder 4 does nothing in orbit mode
            return;
        }
        
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 4 = Playback mode (Stacked/Round Robin/Orbit)
            if (value < 0.33f) {
                ed->playbackMode = 0;  // Stacked
            } else if (value < 0.67f) {
                ed->playbackMode = 1;  // Round Robin
            } else {
                ed->playbackMode = 2;  // Orbit
            }
            ed->audioProcessor.setPlaybackMode(ed->playbackMode);
            // Update parameter display
            float playbackValue = ed->playbackMode == 0 ? 0.0f : (ed->playbackMode == 1 ? 0.5f : 1.0f);
            ed->paramDisplay4.setValue(playbackValue);
            if (ed->playbackMode == 0) {
                ed->paramDisplay4.setValueText("Stacked");
            } else if (ed->playbackMode == 1) {
                ed->paramDisplay4.setValueText("Round Robin");
            } else {
                ed->paramDisplay4.setValueText("Orbit");
            }
        } else {
            // Encoder 4: Sample gain (0.0 to 2.0)
            ed->sampleGain = value * 2.0f;
            // Update current slot's parameter in adapter (this only affects new notes, not existing voices)
            ed->audioProcessor.setSlotSampleGain(ed->currentSlotIndex, ed->sampleGain);
            ed->updateWaveformVisualization();
            // Refresh slot previews to ensure waveform is still visible after gain changes
            ed->updateAllSlotPreviews();
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
        // Check if orbit menu is open - if so, do nothing (encoder 5 disabled in orbit mode)
        if (ed->playbackMode == 2 && ed->orbitMenuOpen) {
            // Encoder 5 does nothing in orbit mode
            return;
        }
        
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 5 = Loop Start Point (0 to sampleLength)
            // Allow loop start to be past loop end for reverse playback
            // Update sampleLength from current slot
            std::vector<float> leftChannel, rightChannel;
            ed->audioProcessor.getSlotStereoSampleDataForVisualization(ed->currentSlotIndex, leftChannel, rightChannel);
            if (!leftChannel.empty()) {
                ed->sampleLength = static_cast<int>(leftChannel.size());
            }
            if (ed->sampleLength > 0) {
                ed->loopStartPoint = static_cast<int>(value * static_cast<float>(ed->sampleLength));
                // No constraint - allow loop start to be past loop end for reverse playback
                // Send to processor for current slot
                ed->audioProcessor.setSlotLoopPoints(ed->currentSlotIndex, ed->loopStartPoint, ed->loopEndPoint);
                // Save state to current slot
                ed->saveCurrentStateToSlot(ed->currentSlotIndex);
                // Update waveform visualization to show loop markers
                ed->updateWaveformVisualization();
                // Update parameter display
                ed->paramDisplay5.setValue(value);
                // Update sampleRate from current slot
                ed->sampleRate = ed->audioProcessor.getSlotSourceSampleRate(ed->currentSlotIndex);
                double loopStartTimeSeconds = static_cast<double>(ed->loopStartPoint) / (ed->sampleRate > 0.0 ? ed->sampleRate : 44100.0);
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
        }
    };
    ed->encoder5.onButtonPressed = [ed]() {
        // Encoder 5 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Loop start default = 0 (value = 0.0)
            ed->encoder5.setValue(0.0f);
            ed->encoder5.onValueChanged(0.0f);
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // Normal mode: Attack default = 0ms (value = 0.0)
            ed->encoder5.setValue(0.0f);
            ed->encoder5.onValueChanged(0.0f);
            // Clear reset flag after value change
            ed->isResettingADSR = false;
        }
    };
}

void EncoderSetupManager::setupEncoder6() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder6.onValueChanged = [ed](float value) {
        // Check if orbit menu is open - if so, do nothing (encoder 6 disabled in orbit mode)
        if (ed->playbackMode == 2 && ed->orbitMenuOpen) {
            return;
        }
        
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 6 = Loop End Point (0 to sampleLength)
            // Update sampleLength from current slot
            std::vector<float> leftChannel, rightChannel;
            ed->audioProcessor.getSlotStereoSampleDataForVisualization(ed->currentSlotIndex, leftChannel, rightChannel);
            if (!leftChannel.empty()) {
                ed->sampleLength = static_cast<int>(leftChannel.size());
            }
            if (ed->sampleLength > 0) {
                ed->loopEndPoint = static_cast<int>(value * static_cast<float>(ed->sampleLength));
                // No constraint - allow loop end to be before loop start for reverse playback
                // Send to processor for current slot
                ed->audioProcessor.setSlotLoopPoints(ed->currentSlotIndex, ed->loopStartPoint, ed->loopEndPoint);
                // Save state to current slot
                ed->saveCurrentStateToSlot(ed->currentSlotIndex);
                // Update waveform visualization to show loop markers
                ed->updateWaveformVisualization();
                // Update parameter display
                ed->paramDisplay6.setValue(value);
                // Update sampleRate from current slot
                ed->sampleRate = ed->audioProcessor.getSlotSourceSampleRate(ed->currentSlotIndex);
                double loopEndTimeSeconds = static_cast<double>(ed->loopEndPoint) / (ed->sampleRate > 0.0 ? ed->sampleRate : 44100.0);
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
        }
    };
    ed->encoder6.onButtonPressed = [ed]() {
        // Encoder 6 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Loop end default = sampleLength (value = 1.0)
            ed->encoder6.setValue(1.0f);
            ed->encoder6.onValueChanged(1.0f);
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // Normal mode: Decay default = 1000ms (value = 0.05)
            ed->encoder6.setValue(0.05f);
            ed->encoder6.onValueChanged(0.05f);
            // Clear reset flag after value change
            ed->isResettingADSR = false;
        }
    };
}

void EncoderSetupManager::setupEncoder7() {
    Op1CloneAudioProcessorEditor* ed = editor;
    ed->encoder7.onValueChanged = [ed](float value) {
        // Check if orbit menu is open - if so, do nothing (encoder 7 disabled in orbit mode)
        if (ed->playbackMode == 2 && ed->orbitMenuOpen) {
            return;
        }
        
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 7 = Loop Enable/Disable (toggle)
            ed->loopEnabled = value >= 0.5f;
            // Send to processor for current slot
            ed->audioProcessor.setSlotLoopEnabled(ed->currentSlotIndex, ed->loopEnabled);
            // Save state to current slot
            ed->saveCurrentStateToSlot(ed->currentSlotIndex);
            // Update waveform visualization to show/hide loop markers
            ed->updateWaveformVisualization();
            // Update parameter display
            ed->paramDisplay7.setValue(value);
            ed->paramDisplay7.setValueText(ed->loopEnabled ? "ON" : "OFF");
        } else {
            // Encoder 7: Sustain (0.0 to 1.0)
            ed->adsrSustain = value;
            // Update current slot's ADSR parameters in adapter (this only affects new notes, not existing voices)
            ed->audioProcessor.setSlotADSR(ed->currentSlotIndex, ed->adsrAttackMs, ed->adsrDecayMs, ed->adsrSustain, ed->adsrReleaseMs);
            // Update visualization only (not engine parameters - those are per-slot now)
            ed->adsrPillComponent.setADSR(ed->adsrAttackMs, ed->adsrDecayMs, ed->adsrSustain, ed->adsrReleaseMs);
            // Update parameter display 7 (Sustain)
            ed->paramDisplay7.setValue(value);
            // Format value: show as percentage (e.g., "50%")
            int sustainPercent = static_cast<int>(ed->adsrSustain * 100.0f);
            ed->paramDisplay7.setValueText(juce::String(sustainPercent) + "%");
            // OLD ADSR visualization show code (COMMENTED OUT - replaced with pill component)
            // Pill component is always visible, so we just update it
            ed->updateADSR();  // Update pill visualization
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
        }
    };
    ed->encoder7.onButtonPressed = [ed]() {
        // Encoder 7 button pressed - reset to default
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Loop enable default = OFF (value = 0.0)
            ed->encoder7.setValue(0.0f);
            ed->encoder7.onValueChanged(0.0f);
        } else {
            // Set flag to prevent showing ADSR visualization during reset
            ed->isResettingADSR = true;
            // OLD hide ADSR visualization code (COMMENTED OUT - replaced with pill component)
            // Pill component stays visible, just update it
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
        // Check if orbit menu is open - if so, do nothing (encoder 8 disabled in orbit mode)
        if (ed->playbackMode == 2 && ed->orbitMenuOpen) {
            return;
        }
        
        if (ed->shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 8 = Playback Mode (Mono/Poly toggle)
            ed->isPolyphonic = value >= 0.5f;
            // Send to processor
            ed->audioProcessor.setPlaybackMode(ed->isPolyphonic);
            // Update parameter display
            ed->paramDisplay8.setValue(value);
            ed->paramDisplay8.setValueText(ed->isPolyphonic ? "POLY" : "MONO");
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

juce::String EncoderSetupManager::getNoteValueString(int noteValue)
{
    // Note values are reversed: 0=4 bars (slowest), 8=1/64 (fastest)
    switch (noteValue) {
        case 0: return "4";      // 4 bars (slowest)
        case 1: return "2";      // 2 bars
        case 2: return "1";      // 1 bar
        case 3: return "1/2";    // Half note
        case 4: return "1/4";    // Quarter note
        case 5: return "1/8";    // Eighth note
        case 6: return "1/16";   // Sixteenth note
        case 7: return "1/32";   // Thirty-second note
        case 8: return "1/64";   // Sixty-fourth note (fastest)
        default: return "1/4";
    }
}
