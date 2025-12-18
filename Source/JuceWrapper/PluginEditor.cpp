#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Core/TimePitchError.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>

Op1CloneAudioProcessorEditor::Op1CloneAudioProcessorEditor(Op1CloneAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
    , encoder1("")
    , encoder2("")
    , encoder3("")
    , encoder4("")
    , encoder5("")
    , encoder6("")
    , encoder7("")
    , encoder8("") {
    
    // Setup screen component
    addAndMakeVisible(&screenComponent);
    
    // Setup ADSR visualization (overlay on screen)
    // Don't add to component tree until it's needed - this prevents any painting on startup
    adsrVisualization.setAlwaysOnTop(true);
    adsrVisualization.setAlpha(0.0f);  // Start invisible
    adsrVisualization.setVisible(false);  // Start hidden
    adsrVisualization.setInterceptsMouseClicks(false, false);  // Don't intercept mouse clicks
    // Don't add to component tree yet - will be added when first shown
    
    // Setup ADSR label (overlay in top right of screen)
    adsrLabel.setText("ADSR", juce::dontSendNotification);
    adsrLabel.setJustificationType(juce::Justification::centredRight);
    adsrLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    adsrLabel.setAlwaysOnTop(true);
    adsrLabel.setVisible(false);  // Hidden by default, shown when shift is enabled
    addChildComponent(&adsrLabel);  // Use addChildComponent so it starts hidden
    
    // Setup parameter display label (overlay in top left of screen)
    parameterDisplayLabel.setText("", juce::dontSendNotification);
    parameterDisplayLabel.setJustificationType(juce::Justification::centredLeft);
    parameterDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    parameterDisplayLabel.setAlwaysOnTop(true);
    parameterDisplayLabel.setVisible(false);  // Hidden by default
    addAndMakeVisible(&parameterDisplayLabel);
    
    // Setup BPM display label (overlay in top right of screen)
    projectBPM = 120;  // Default BPM
    bpmDisplayLabel.setText("BPM: 120", juce::dontSendNotification);
    bpmDisplayLabel.setJustificationType(juce::Justification::centredRight);
    bpmDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    bpmDisplayLabel.setAlwaysOnTop(true);
    bpmDisplayLabel.setVisible(true);  // Always visible
    addAndMakeVisible(&bpmDisplayLabel);
    
    // Initialize fade-out tracking
    lastEncoderChangeTime = 0;
    parameterDisplayAlpha = 0.0f;
    currentParameterText = "";
    
    // Initialize sample editing parameters
    repitchSemitones = 0.0f;
    startPoint = 0;
    endPoint = 0;
    sampleGain = 1.0f;
    sampleLength = 0;
    
    // Setup MIDI status component
    addAndMakeVisible(&midiStatusComponent);
    midiStatusComponent.setMidiHandler(&audioProcessor.getMidiInputHandler());
    
    // Setup gain slider
    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainSlider.setRange(0.0, 1.0, 0.01);
    gainSlider.setValue(1.0);
    addAndMakeVisible(&gainSlider);
    
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "gain", gainSlider);
    
    // Setup gain label (removed - will use volumeLabel instead)
    
    // Setup volume label (text under the knob)
    volumeLabel.setText("Volume", juce::dontSendNotification);
    volumeLabel.setJustificationType(juce::Justification::centred);
    volumeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(&volumeLabel);
    
    // Setup sample name label (overlay on top of screen)
    sampleNameLabel.setText("Default (440Hz tone)", juce::dontSendNotification);
    sampleNameLabel.setJustificationType(juce::Justification::centredLeft);
    sampleNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(&sampleNameLabel);
    sampleNameLabel.setAlwaysOnTop(true);
    
    // Setup parameter displays at bottom of screen
    paramDisplay1.setLabel("PITCH");
    paramDisplay2.setLabel("START");
    paramDisplay3.setLabel("END");
    paramDisplay4.setLabel("GAIN");
    paramDisplay5.setLabel("ATTACK");
    paramDisplay6.setLabel("DECAY");
    paramDisplay7.setLabel("SUSTAIN");
    paramDisplay8.setLabel("RELEASE");
    
    paramDisplay1.setAlwaysOnTop(true);
    paramDisplay2.setAlwaysOnTop(true);
    paramDisplay3.setAlwaysOnTop(true);
    paramDisplay4.setAlwaysOnTop(true);
    paramDisplay5.setAlwaysOnTop(true);
    paramDisplay6.setAlwaysOnTop(true);
    paramDisplay7.setAlwaysOnTop(true);
    paramDisplay8.setAlwaysOnTop(true);
    
    addAndMakeVisible(&paramDisplay1);
    addAndMakeVisible(&paramDisplay2);
    addAndMakeVisible(&paramDisplay3);
    addAndMakeVisible(&paramDisplay4);
    addAndMakeVisible(&paramDisplay5);
    addAndMakeVisible(&paramDisplay6);
    addAndMakeVisible(&paramDisplay7);
    addAndMakeVisible(&paramDisplay8);
    
    // Setup load sample button
    loadSampleButton.setButtonText("Load Sample...");
    loadSampleButton.onClick = [this] { loadSampleButtonClicked(); };
    addAndMakeVisible(&loadSampleButton);
    
    // Time-warp toggle (larger, more visible)
    warpToggleButton.setButtonText("Time Warp");
    warpToggleButton.setToggleState(true, juce::dontSendNotification);
    warpToggleButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    warpToggleButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::lightgreen);
    warpToggleButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    // Use Button::Listener pattern for reliable state changes
    warpToggleButton.addListener(this);
    // Also set initial state
    audioProcessor.setTimeWarpEnabled(true);
    addAndMakeVisible(&warpToggleButton);
    
    // Shift toggle button (square, lit up when enabled, greyed out when disabled)
    shiftToggleButton.setButtonText("shift");
    shiftToggleButton.setClickingTogglesState(true);
    shiftToggleButton.setToggleState(false, juce::dontSendNotification);
    shiftToggleButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    shiftToggleButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::lightblue);
    shiftToggleButton.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
    shiftToggleButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    shiftToggleButton.addListener(this);
    addAndMakeVisible(&shiftToggleButton);
    
    // Info label removed
    
    currentSampleName = "Default (440Hz tone)";
    
    // Enable keyboard focus for MIDI keyboard input
    setWantsKeyboardFocus(true);
    
    // Start timer to update error status (every 100ms)
    startTimer(100);
    
    // Update waveform with default sample
    updateWaveform();
    
    // ADSR parameters (in milliseconds, except sustain which is 0.0-1.0)
    adsrAttackMs = 2.0f;
    adsrDecayMs = 0.0f;
    adsrSustain = 1.0f;
    adsrReleaseMs = 20.0f;
    
    // ADSR visualization fade-out tracking
    isADSRDragging = false;
    adsrFadeOutStartTime = 0;
    adsrVisualization.setAlpha(0.0f);  // Start invisible
    adsrVisualization.setVisible(false);  // Start hidden
    
    // Initialize parameter displays with default values
    // Pitch: 0 semitones = 0.5 normalized
    paramDisplay1.setValue(0.5f);
    paramDisplay1.setValueText("+0");
    // Start: 0 = 0.0 normalized
    paramDisplay2.setValue(0.0f);
    paramDisplay2.setValueText("0ms");
    // End: full length = 1.0 normalized (will be updated after sample loads)
    paramDisplay3.setValue(1.0f);
    paramDisplay3.setValueText("0ms");  // Will be updated when sample loads
    // Gain: 1.0x = 0.5 normalized
    paramDisplay4.setValue(0.5f);
    paramDisplay4.setValueText("1.00x");
    // Attack: 2ms = 0.0002 normalized
    paramDisplay5.setValue(0.0002f);
    paramDisplay5.setValueText("2ms");
    // Decay: 0ms = 0.0 normalized
    paramDisplay6.setValue(0.0f);
    paramDisplay6.setValueText("0ms");
    // Sustain: 1.0 = 1.0 normalized
    paramDisplay7.setValue(1.0f);
    paramDisplay7.setValueText("100%");
    // Release: 20ms = 0.001 normalized
    paramDisplay8.setValue(0.001f);
    paramDisplay8.setValueText("20ms");
    
    // Initialize encoder 5-8 to ADSR defaults
    // Note: setValue already uses dontSendNotification, so callbacks won't fire
    encoder5.setValue(0.0002f); // Attack default
    encoder6.setValue(0.0f);    // Decay default
    encoder7.setValue(1.0f);    // Sustain default
    encoder8.setValue(0.001f);  // Release default
    
    // Explicitly ensure ADSR visualization is hidden after initialization
    adsrVisualization.setAlpha(0.0f);
    adsrVisualization.setVisible(false);
    isADSRDragging = false;
    
    // Setup encoders - ADSR is now on encoders 5-8 (shift off), top 4 do nothing in shift mode
    encoder1.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 1 does nothing (for now)
        } else {
            // Encoder 1: Repitch (-24 to +24 semitones)
            repitchSemitones = (value - 0.5f) * 48.0f; // Map 0-1 to -24 to +24
            updateSampleEditing();
            // Update parameter display 1 (Pitch) - normalize from -24/+24 to 0-1
            float normalizedPitch = (repitchSemitones + 24.0f) / 48.0f;
            paramDisplay1.setValue(normalizedPitch);
            // Format value: show semitones with +/- sign
            int semitones = static_cast<int>(std::round(repitchSemitones));
            juce::String pitchText = (semitones >= 0 ? "+" : "") + juce::String(semitones);
            paramDisplay1.setValueText(pitchText);
        }
    };
    encoder1.onButtonPressed = [this]() {
        // Encoder 1 button pressed - reset to default
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 1 does nothing (for now)
        } else {
            // Normal mode: Repitch default = 0 semitones (value = 0.5)
            encoder1.setValue(0.5f);
            encoder1.onValueChanged(0.5f);
        }
    };
    
    encoder2.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 2 does nothing (for now)
        } else {
            // Encoder 2: Start point (0 to sampleLength)
            if (sampleLength > 0) {
                startPoint = static_cast<int>(value * static_cast<float>(sampleLength));
                // Ensure start point is before end point
                if (startPoint >= endPoint) {
                    startPoint = std::max(0, endPoint - 1);
                }
                updateSampleEditing();
                updateWaveformVisualization();
                // Force repaint
                screenComponent.repaint();
                // Update parameter display 2 (Start)
                paramDisplay2.setValue(value);
                // Format value: show as time position (e.g., "1.5s" or "500ms")
                double startTimeSeconds = static_cast<double>(startPoint) / sampleRate;
                if (startTimeSeconds >= 1.0) {
                    paramDisplay2.setValueText(juce::String(startTimeSeconds, 2) + "s");
                } else {
                    paramDisplay2.setValueText(juce::String(static_cast<int>(startTimeSeconds * 1000.0)) + "ms");
                }
            }
        }
    };
    encoder2.onButtonPressed = [this]() {
        // Encoder 2 button pressed - reset to default
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 2 does nothing (for now)
        } else {
            // Normal mode: Start point default = 0 (value = 0.0)
            encoder2.setValue(0.0f);
            encoder2.onValueChanged(0.0f);
        }
    };
    
    encoder3.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 3 does nothing (for now)
        } else {
            // Encoder 3: End point (0 to sampleLength)
            if (sampleLength > 0) {
                endPoint = static_cast<int>(value * static_cast<float>(sampleLength));
                // Ensure end point is after start point
                if (endPoint <= startPoint) {
                    endPoint = std::min(sampleLength, startPoint + 1);
                }
                updateSampleEditing();
                updateWaveformVisualization();
                // Force repaint
                screenComponent.repaint();
                // Update parameter display 3 (End)
                paramDisplay3.setValue(value);
                // Format value: show as time position (e.g., "1.5s" or "500ms")
                double endTimeSeconds = static_cast<double>(endPoint) / sampleRate;
                if (endTimeSeconds >= 1.0) {
                    paramDisplay3.setValueText(juce::String(endTimeSeconds, 2) + "s");
                } else {
                    paramDisplay3.setValueText(juce::String(static_cast<int>(endTimeSeconds * 1000.0)) + "ms");
                }
            }
        }
    };
    encoder3.onButtonPressed = [this]() {
        // Encoder 3 button pressed - reset to default
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 3 does nothing (for now)
        } else {
            // Normal mode: End point default = full length (value = 1.0)
            encoder3.setValue(1.0f);
            encoder3.onValueChanged(1.0f);
        }
    };
    
    encoder4.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 4 does nothing (for now)
        } else {
            // Encoder 4: Sample gain (0.0 to 2.0)
            sampleGain = value * 2.0f;
            updateSampleEditing();
            updateWaveformVisualization();
            // Update parameter display 4 (Gain) - normalize from 0-2.0 to 0-1
            paramDisplay4.setValue(value);
            // Format value: show as multiplier (e.g., "1.5x")
            paramDisplay4.setValueText(juce::String(sampleGain, 2) + "x");
        }
    };
    encoder4.onButtonPressed = [this]() {
        // Encoder 4 button pressed - reset to default
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 4 does nothing (for now)
        } else {
            // Normal mode: Sample gain default = 1.0x (value = 0.5)
            encoder4.setValue(0.5f);
            encoder4.onValueChanged(0.5f);
        }
    };
    
    // Setup encoders 5-8 - control ADSR when shift is OFF
    encoder5.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 5 does nothing (for now)
        } else {
            // Encoder 5: Attack (0-10000ms = 0-10 seconds)
            adsrAttackMs = value * 10000.0f;
            updateADSR();
            // Update parameter display 5 (Attack)
            paramDisplay5.setValue(value);
            // Format value: show in ms, or seconds if >= 1000ms
            if (adsrAttackMs >= 1000.0f) {
                paramDisplay5.setValueText(juce::String(adsrAttackMs / 1000.0f, 2) + "s");
            } else {
                paramDisplay5.setValueText(juce::String(static_cast<int>(adsrAttackMs)) + "ms");
            }
            // Show ADSR visualization when value changes (only if not already showing)
            if (!isADSRDragging) {
                isADSRDragging = true;
                // Add to component tree if not already added
                if (adsrVisualization.getParentComponent() == nullptr) {
                    addAndMakeVisible(&adsrVisualization);
                }
                // Make visible first, then update
                adsrVisualization.setAlpha(1.0f);
                adsrVisualization.setVisible(true);
                updateADSR();  // Ensure values are up to date
                repaint();
            }
            // Reset fade-out timer (will be set on drag end)
            adsrFadeOutStartTime = 0;
        }
    };
    encoder5.onDragStart = [this]() {
        if (!shiftToggleButton.getToggleState()) {
            isADSRDragging = true;
            // Add to component tree if not already added
            if (adsrVisualization.getParentComponent() == nullptr) {
                addAndMakeVisible(&adsrVisualization);
            }
            // Make visible first, then update
            adsrVisualization.setAlpha(1.0f);
            adsrVisualization.setVisible(true);
            updateADSR();  // Update with current values
            repaint();  // Repaint the entire editor to ensure visualization shows
        }
    };
    encoder5.onDragEnd = [this]() {
        if (!shiftToggleButton.getToggleState()) {
            isADSRDragging = false;
            adsrFadeOutStartTime = juce::Time::currentTimeMillis();
        }
    };
    encoder5.onButtonPressed = [this]() {
        // Encoder 5 button pressed - reset to default
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 5 does nothing (for now)
        } else {
            // Normal mode: Attack default = 2.0ms (value = 0.0002)
            encoder5.setValue(0.0002f);
            encoder5.onValueChanged(0.0002f);
        }
    };
    
    encoder6.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 6 does nothing (for now)
        } else {
            // Encoder 6: Decay (0-20000ms = 0-20 seconds)
            adsrDecayMs = value * 20000.0f;
            updateADSR();
            // Update parameter display 6 (Decay)
            paramDisplay6.setValue(value);
            // Format value: show in ms, or seconds if >= 1000ms
            if (adsrDecayMs >= 1000.0f) {
                paramDisplay6.setValueText(juce::String(adsrDecayMs / 1000.0f, 2) + "s");
            } else {
                paramDisplay6.setValueText(juce::String(static_cast<int>(adsrDecayMs)) + "ms");
            }
            // Show ADSR visualization when value changes (only if not already showing)
            if (!isADSRDragging) {
                isADSRDragging = true;
                // Add to component tree if not already added
                if (adsrVisualization.getParentComponent() == nullptr) {
                    addAndMakeVisible(&adsrVisualization);
                }
                // Make visible first, then update
                adsrVisualization.setAlpha(1.0f);
                adsrVisualization.setVisible(true);
                updateADSR();  // Ensure values are up to date
                repaint();
            }
            // Reset fade-out timer (will be set on drag end)
            adsrFadeOutStartTime = 0;
        }
    };
    encoder6.onDragStart = [this]() {
        if (!shiftToggleButton.getToggleState()) {
            isADSRDragging = true;
            // Add to component tree if not already added
            if (adsrVisualization.getParentComponent() == nullptr) {
                addAndMakeVisible(&adsrVisualization);
            }
            // Make visible first, then update
            adsrVisualization.setAlpha(1.0f);
            adsrVisualization.setVisible(true);
            updateADSR();  // Update with current values
            repaint();  // Repaint the entire editor to ensure visualization shows
        }
    };
    encoder6.onDragEnd = [this]() {
        if (!shiftToggleButton.getToggleState()) {
            isADSRDragging = false;
            adsrFadeOutStartTime = juce::Time::currentTimeMillis();
        }
    };
    encoder6.onButtonPressed = [this]() {
        // Encoder 6 button pressed - reset to default
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 6 does nothing (for now)
        } else {
            // Normal mode: Decay default = 0.0ms (value = 0.0)
            encoder6.setValue(0.0f);
            encoder6.onValueChanged(0.0f);
        }
    };
    
    encoder7.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 7 does nothing (for now)
        } else {
            // Encoder 7: Sustain (0.0-1.0)
            adsrSustain = value;
            updateADSR();
            // Update parameter display 7 (Sustain)
            paramDisplay7.setValue(value);
            // Format value: show as percentage
            int sustainPercent = static_cast<int>(adsrSustain * 100.0f);
            paramDisplay7.setValueText(juce::String(sustainPercent) + "%");
            // Show ADSR visualization when value changes (only if not already showing)
            if (!isADSRDragging) {
                isADSRDragging = true;
                // Add to component tree if not already added
                if (adsrVisualization.getParentComponent() == nullptr) {
                    addAndMakeVisible(&adsrVisualization);
                }
                // Make visible first, then update
                adsrVisualization.setAlpha(1.0f);
                adsrVisualization.setVisible(true);
                updateADSR();  // Ensure values are up to date
                repaint();
            }
            // Reset fade-out timer (will be set on drag end)
            adsrFadeOutStartTime = 0;
        }
    };
    encoder7.onDragStart = [this]() {
        if (!shiftToggleButton.getToggleState()) {
            isADSRDragging = true;
            // Add to component tree if not already added
            if (adsrVisualization.getParentComponent() == nullptr) {
                addAndMakeVisible(&adsrVisualization);
            }
            // Make visible first, then update
            adsrVisualization.setAlpha(1.0f);
            adsrVisualization.setVisible(true);
            updateADSR();  // Update with current values
            repaint();  // Repaint the entire editor to ensure visualization shows
        }
    };
    encoder7.onDragEnd = [this]() {
        if (!shiftToggleButton.getToggleState()) {
            isADSRDragging = false;
            adsrFadeOutStartTime = juce::Time::currentTimeMillis();
        }
    };
    encoder7.onButtonPressed = [this]() {
        // Encoder 7 button pressed - reset to default
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 7 does nothing (for now)
        } else {
            // Normal mode: Sustain default = 1.0 (value = 1.0)
            encoder7.setValue(1.0f);
            encoder7.onValueChanged(1.0f);
        }
    };
    
    encoder8.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 8 does nothing (for now)
        } else {
            // Encoder 8: Release (0-20000ms = 0-20 seconds)
            adsrReleaseMs = value * 20000.0f;
            updateADSR();
            // Update parameter display 8 (Release)
            paramDisplay8.setValue(value);
            // Format value: show in ms, or seconds if >= 1000ms
            if (adsrReleaseMs >= 1000.0f) {
                paramDisplay8.setValueText(juce::String(adsrReleaseMs / 1000.0f, 2) + "s");
            } else {
                paramDisplay8.setValueText(juce::String(static_cast<int>(adsrReleaseMs)) + "ms");
            }
            // Show ADSR visualization when value changes (only if not already showing)
            if (!isADSRDragging) {
                isADSRDragging = true;
                // Add to component tree if not already added
                if (adsrVisualization.getParentComponent() == nullptr) {
                    addAndMakeVisible(&adsrVisualization);
                }
                // Make visible first, then update
                adsrVisualization.setAlpha(1.0f);
                adsrVisualization.setVisible(true);
                updateADSR();  // Ensure values are up to date
                repaint();
            }
            // Reset fade-out timer (will be set on drag end)
            adsrFadeOutStartTime = 0;
        }
    };
    encoder8.onDragStart = [this]() {
        if (!shiftToggleButton.getToggleState()) {
            isADSRDragging = true;
            // Add to component tree if not already added
            if (adsrVisualization.getParentComponent() == nullptr) {
                addAndMakeVisible(&adsrVisualization);
            }
            // Make visible first, then update
            adsrVisualization.setAlpha(1.0f);
            adsrVisualization.setVisible(true);
            updateADSR();  // Update with current values
            repaint();  // Repaint the entire editor to ensure visualization shows
        }
    };
    encoder8.onDragEnd = [this]() {
        if (!shiftToggleButton.getToggleState()) {
            isADSRDragging = false;
            adsrFadeOutStartTime = juce::Time::currentTimeMillis();
        }
    };
    encoder8.onButtonPressed = [this]() {
        // Encoder 8 button pressed - reset to default
        if (shiftToggleButton.getToggleState()) {
            // Shift mode: Encoder 8 does nothing (for now)
        } else {
            // Normal mode: Release default = 20.0ms (value = 0.001)
            encoder8.setValue(0.001f);
            encoder8.onValueChanged(0.001f);
        }
    };
    
    // Make encoders visible
    addAndMakeVisible(&encoder1);
    addAndMakeVisible(&encoder2);
    addAndMakeVisible(&encoder3);
    addAndMakeVisible(&encoder4);
    addAndMakeVisible(&encoder5);
    addAndMakeVisible(&encoder6);
    addAndMakeVisible(&encoder7);
    addAndMakeVisible(&encoder8);
    
    setSize(1200, 500); // Double width
}

Op1CloneAudioProcessorEditor::~Op1CloneAudioProcessorEditor() {
    stopTimer();
}

void Op1CloneAudioProcessorEditor::paint(juce::Graphics& g) {
    // Dark background
    g.fillAll(juce::Colour(0xFF2A2A2A));
}

void Op1CloneAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();
    
    // Top left: Master gain knob
    auto topLeftArea = bounds.removeFromLeft(150).removeFromTop(150).reduced(10);
    auto gainArea = topLeftArea;
    gainSlider.setBounds(gainArea.removeFromTop(120).reduced(10));
    volumeLabel.setBounds(gainArea.reduced(5));  // "Volume" text under the knob
    
    // Left side: Time Warp and Shift toggles (before screen)
    auto leftControlsArea = bounds.removeFromLeft(200).reduced(10);
    
    // Time-warp toggle
    warpToggleButton.setBounds(leftControlsArea.removeFromTop(40).reduced(5));
    
    // Shift toggle button (wide enough for "shift" text)
    auto shiftButtonArea = leftControlsArea.removeFromTop(50);
    int shiftButtonWidth = 80;  // Wide enough for "shift" text
    shiftToggleButton.setBounds(shiftButtonArea.removeFromLeft(shiftButtonWidth).reduced(5));
    
    // Middle: Screen component (30% wider than before)
    auto screenArea = bounds.removeFromLeft(bounds.getWidth() * 0.65 * 0.78); // 65% * 78% = ~51% (30% wider than 39%)
    auto screenBounds = screenArea.removeFromTop(static_cast<int>(screenArea.getHeight() * 0.6)); // 40% reduction = 60% of original
    screenComponent.setBounds(screenBounds.reduced(10));
    
    // Sample name label (overlay on top of screen)
    auto screenComponentBounds = screenComponent.getBounds();
    sampleNameLabel.setBounds(screenComponentBounds.getX() + 10, 
                              screenComponentBounds.getY() + 5, 
                              screenComponentBounds.getWidth() - 20, 
                              20);
    
    // Parameter displays at bottom of screen module (2 rows of 4)
    // Position them at the very bottom, ensuring they don't overlap the waveform
    // Add 5px padding around the group
    int paramDisplayHeight = 40;
    int paramDisplaySpacing = 5;
    int paramGroupPadding = 5; // Padding around the entire group
    int paramBottomPadding = 5; // Padding from bottom edge
    int totalParamHeight = paramDisplayHeight * 2 + paramDisplaySpacing;
    
    // Calculate available width for displays (with group padding)
    int availableWidth = screenComponentBounds.getWidth() - (paramGroupPadding * 2);
    int paramDisplayWidth = (availableWidth - paramDisplaySpacing * 3) / 4;
    
    // Calculate X start position (with left padding)
    int paramStartX = screenComponentBounds.getX() + paramGroupPadding;
    
    // Calculate Y positions from the bottom of the screen component
    int paramBottomRowY = screenComponentBounds.getBottom() - paramBottomPadding - paramDisplayHeight;
    int paramTopRowY = paramBottomRowY - paramDisplayHeight - paramDisplaySpacing;
    
    // Top row: Pitch, Start, End, Gain
    paramDisplay1.setBounds(paramStartX, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    paramDisplay2.setBounds(paramStartX + paramDisplayWidth + paramDisplaySpacing, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    paramDisplay3.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 2, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    paramDisplay4.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 3, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    
    // Bottom row: Attack, Decay, Sustain, Release
    paramDisplay5.setBounds(paramStartX, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    paramDisplay6.setBounds(paramStartX + paramDisplayWidth + paramDisplaySpacing, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    paramDisplay7.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 2, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    paramDisplay8.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 3, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    
    // ADSR visualization overlay - centered on waveform area within screen component
    // Get waveform bounds from screen component (relative to screen component)
    auto waveformBounds = screenComponent.getWaveformBounds();
    // Convert to editor coordinates (waveformBounds is relative to screenComponent, need to add screenComponent position)
    juce::Rectangle<int> waveformBoundsInEditor(
        screenComponentBounds.getX() + waveformBounds.getX(),
        screenComponentBounds.getY() + waveformBounds.getY(),
        waveformBounds.getWidth(),
        waveformBounds.getHeight()
    );
    int adsrHeight = static_cast<int>(waveformBoundsInEditor.getHeight() * 0.4f);  // 40% of waveform height
    // Center it vertically on the waveform
    int adsrY = waveformBoundsInEditor.getCentreY() - adsrHeight / 2;
    adsrVisualization.setBounds(waveformBoundsInEditor.getX(),
                                adsrY,
                                waveformBoundsInEditor.getWidth(),
                                adsrHeight);
    
    // ADSR label (overlay in top right of screen)
    adsrLabel.setBounds(screenComponentBounds.getX() + screenComponentBounds.getWidth() - 60,
                        screenComponentBounds.getY() + 5,
                        50,
                        20);
    
    // Parameter display label (overlay in top left of screen)
    parameterDisplayLabel.setBounds(screenComponentBounds.getX() + 10,
                                    screenComponentBounds.getY() + 5,
                                    100,
                                    20);
    
    // BPM display label (overlay in top right of screen)
    bpmDisplayLabel.setBounds(screenComponentBounds.getX() + screenComponentBounds.getWidth() - 100,
                               screenComponentBounds.getY() + 5,
                               90,
                               20);
    
    // Under screen: Load sample button (directly below the screen)
    loadSampleButton.setBounds(screenArea.removeFromTop(40).reduced(10));
    
        // Right side: Encoders in two horizontal rows
        auto encoderArea = bounds.reduced(10);
        int encoderSize = 80; // Much smaller encoders
        int encoderSpacing = 15;
        int rowSpacing = 20; // Vertical spacing between rows
        // Move left (closer to screen) - start closer to left edge instead of centering
        int startX = encoderArea.getX() + 20; // 20px from left edge instead of centered
        
        // Top row (encoders 1-4)
        int topRowY = encoderArea.getY() + encoderArea.getHeight() * 0.15; // Move up more (15% from top, was 25%)
        encoder1.setBounds(startX, topRowY - encoderSize / 2, encoderSize, encoderSize);
        encoder2.setBounds(startX + encoderSize + encoderSpacing, topRowY - encoderSize / 2, encoderSize, encoderSize);
        encoder3.setBounds(startX + (encoderSize + encoderSpacing) * 2, topRowY - encoderSize / 2, encoderSize, encoderSize);
        encoder4.setBounds(startX + (encoderSize + encoderSpacing) * 3, topRowY - encoderSize / 2, encoderSize, encoderSize);
        
        // Bottom row (encoders 5-8)
        int bottomRowY = topRowY + encoderSize + rowSpacing;
        encoder5.setBounds(startX, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
        encoder6.setBounds(startX + encoderSize + encoderSpacing, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
        encoder7.setBounds(startX + (encoderSize + encoderSpacing) * 2, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
        encoder8.setBounds(startX + (encoderSize + encoderSpacing) * 3, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
    
    // MIDI status at bottom
    auto statusBounds = getLocalBounds().removeFromBottom(25);
    midiStatusComponent.setBounds(statusBounds);
}

void Op1CloneAudioProcessorEditor::loadSampleButtonClicked() {
    // Open file picker for WAV files
    // FileChooser must be stored as member to stay alive during async operation
    fileChooser = std::make_unique<juce::FileChooser>("Select a WAV sample file...",
                                                      juce::File(),
                                                      "*.wav;*.aif;*.aiff");
    
    auto chooserFlags = juce::FileBrowserComponent::openMode | 
                        juce::FileBrowserComponent::canSelectFiles;
    
    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        auto selectedFile = fc.getResult();
        
        if (selectedFile.existsAsFile()) {
            if (audioProcessor.loadSampleFromFile(selectedFile)) {
                // Update UI
                currentSampleName = selectedFile.getFileName();
                sampleNameLabel.setText(currentSampleName, juce::dontSendNotification);
                
                // Update waveform visualization
                updateWaveform();
                
                repaint();
            } else {
                // Error loading - could show a message box or just continue
                repaint();
            }
        }
        
        // Clear file chooser after use
        fileChooser.reset();
    });
}

bool Op1CloneAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    int note = keyToMidiNote(key.getKeyCode());
    if (note >= 0 && note < 128 && !pressedKeys[note]) {
        pressedKeys[note] = true;
        sendMidiNote(note, 1.0f, true);
        return true;
    }
    return false;
}

bool Op1CloneAudioProcessorEditor::keyStateChanged(bool isKeyDown) {
    // Check all mapped keyboard keys and send note off for released keys
    // Standard piano keyboard layout
    int keyCodes[] = {
        'A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y', 'H', 'U', 'J', 'K',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/'  // Lower octave
    };
    int baseNotes[] = {
        60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72,  // Middle octave
        48, 50, 52, 53, 55, 57, 59, 60, 62, 64  // Lower octave (white keys only for lower)
    };
    
    for (size_t i = 0; i < sizeof(keyCodes) / sizeof(keyCodes[0]); ++i) {
        int note = baseNotes[i];
        bool keyCurrentlyDown = juce::KeyPress::isKeyCurrentlyDown(keyCodes[i]);
        
        if (!keyCurrentlyDown && note >= 0 && note < 128 && pressedKeys[note]) {
            // Key was released
            pressedKeys[note] = false;
            sendMidiNote(note, 0.0f, false);
        }
    }
    return true;
}

int Op1CloneAudioProcessorEditor::keyToMidiNote(int keyCode) const {
    // Standard piano keyboard layout starting at C4 (MIDI note 60)
    // White keys: A=60, S=62, D=64, F=65, G=67, H=69, J=71, K=72
    // Black keys: W=61, E=63, T=66, Y=68, U=70
    // Lower octave: Z=48, X=50, C=52, V=53, B=55, N=57, M=59, ,=60, .=62, /=64
    
    switch (keyCode) {
        // Middle octave (C4-C5)
        case 'A': return 60; // C
        case 'W': return 61; // C#
        case 'S': return 62; // D
        case 'E': return 63; // D#
        case 'D': return 64; // E
        case 'F': return 65; // F
        case 'T': return 66; // F#
        case 'G': return 67; // G
        case 'Y': return 68; // G#
        case 'H': return 69; // A
        case 'U': return 70; // A#
        case 'J': return 71; // B
        case 'K': return 72; // C
        
        // Lower octave (C3-C4)
        case 'Z': return 48; // C
        case 'X': return 50; // D
        case 'C': return 52; // E
        case 'V': return 53; // F
        case 'B': return 55; // G
        case 'N': return 57; // A
        case 'M': return 59; // B
        case ',': return 60; // C
        case '.': return 62; // D
        case '/': return 64; // E
        
        default: return -1;
    }
}

void Op1CloneAudioProcessorEditor::sendMidiNote(int note, float velocity, bool noteOn) {
    // Send MIDI message through processor
    audioProcessor.sendMidiNote(note, velocity, noteOn);
}

void Op1CloneAudioProcessorEditor::timerCallback() {
    // Handle parameter display fade-out
    if (parameterDisplayAlpha > 0.0f) {
        int64_t currentTime = juce::Time::currentTimeMillis();
        int64_t timeSinceLastChange = currentTime - lastEncoderChangeTime;
        
        // If 1 second (1000ms) has passed since last encoder change, start fading out
        if (timeSinceLastChange > 1000) {
            // Fade out over 1 second (100ms timer = 10 steps)
            parameterDisplayAlpha -= 0.1f;
            if (parameterDisplayAlpha < 0.0f) {
                parameterDisplayAlpha = 0.0f;
                parameterDisplayLabel.setVisible(false);
            } else {
                // Update label color with alpha
                parameterDisplayLabel.setColour(juce::Label::textColourId, 
                    juce::Colours::white.withAlpha(parameterDisplayAlpha));
                parameterDisplayLabel.repaint();
            }
        }
    }
    
    // Handle ADSR visualization fade-out
    if (!isADSRDragging && adsrFadeOutStartTime > 0) {
        int64_t currentTime = juce::Time::currentTimeMillis();
        int64_t elapsed = currentTime - adsrFadeOutStartTime;
        
        if (elapsed >= ADSR_FADE_OUT_DURATION_MS) {
            // Fade complete - hide
            adsrVisualization.setAlpha(0.0f);
            adsrVisualization.setVisible(false);
            adsrFadeOutStartTime = 0;
        } else {
            // Calculate fade progress (0.0 to 1.0)
            float fadeProgress = static_cast<float>(elapsed) / static_cast<float>(ADSR_FADE_OUT_DURATION_MS);
            float alpha = 1.0f - fadeProgress;
            adsrVisualization.setAlpha(alpha);
        }
    }
}

void Op1CloneAudioProcessorEditor::updateWaveform() {
    // Get sample data from processor and update screen
    std::vector<float> sampleData;
    audioProcessor.getSampleDataForVisualization(sampleData);
    screenComponent.setSampleData(sampleData);
    
    // Get source sample rate from processor (for time calculations)
    sampleRate = audioProcessor.getSourceSampleRate();
    
    // Ensure ADSR visualization is hidden on startup/update
    adsrVisualization.setAlpha(0.0f);
    adsrVisualization.setVisible(false);
    isADSRDragging = false;
    adsrFadeOutStartTime = 0;
    
    // Update sample length for encoder mapping
    int newSampleLength = static_cast<int>(sampleData.size());
    
    // If this is a new sample (different length) or first load, reset to full sample view
    bool isNewSample = (newSampleLength != sampleLength);
    
    if (newSampleLength > 0) {
        if (isNewSample || endPoint == 0 || endPoint > newSampleLength || startPoint >= endPoint) {
            // Reset to show full sample
            startPoint = 0;
            endPoint = newSampleLength;
            
            // Update encoder positions to match (encoder2 = 0.0 for start, encoder3 = 1.0 for end)
            encoder2.setValue(0.0f);
            encoder3.setValue(1.0f);
        }
        
        sampleLength = newSampleLength;
        
        // Update the processor with initial values
        updateSampleEditing();
        updateWaveformVisualization();
        
        // Initialize parameter displays with current values
        float normalizedPitch = (repitchSemitones + 24.0f) / 48.0f;
        paramDisplay1.setValue(normalizedPitch);
        int semitones = static_cast<int>(std::round(repitchSemitones));
        paramDisplay1.setValueText((semitones >= 0 ? "+" : "") + juce::String(semitones));
        
        float startValue = startPoint > 0 ? static_cast<float>(startPoint) / static_cast<float>(sampleLength) : 0.0f;
        paramDisplay2.setValue(startValue);
        double startTimeSeconds = static_cast<double>(startPoint) / sampleRate;
        if (startTimeSeconds >= 1.0) {
            paramDisplay2.setValueText(juce::String(startTimeSeconds, 2) + "s");
        } else {
            paramDisplay2.setValueText(juce::String(static_cast<int>(startTimeSeconds * 1000.0)) + "ms");
        }
        
        float endValue = endPoint > 0 ? static_cast<float>(endPoint) / static_cast<float>(sampleLength) : 1.0f;
        paramDisplay3.setValue(endValue);
        double endTimeSeconds = static_cast<double>(endPoint) / sampleRate;
        if (endTimeSeconds >= 1.0) {
            paramDisplay3.setValueText(juce::String(endTimeSeconds, 2) + "s");
        } else {
            paramDisplay3.setValueText(juce::String(static_cast<int>(endTimeSeconds * 1000.0)) + "ms");
        }
        
        paramDisplay4.setValue(sampleGain / 2.0f);
        paramDisplay4.setValueText(juce::String(sampleGain, 2) + "x");
        
        float attackValue = adsrAttackMs / 10000.0f;
        paramDisplay5.setValue(attackValue);
        if (adsrAttackMs >= 1000.0f) {
            paramDisplay5.setValueText(juce::String(adsrAttackMs / 1000.0f, 2) + "s");
        } else {
            paramDisplay5.setValueText(juce::String(static_cast<int>(adsrAttackMs)) + "ms");
        }
        
        float decayValue = adsrDecayMs / 20000.0f;
        paramDisplay6.setValue(decayValue);
        if (adsrDecayMs >= 1000.0f) {
            paramDisplay6.setValueText(juce::String(adsrDecayMs / 1000.0f, 2) + "s");
        } else {
            paramDisplay6.setValueText(juce::String(static_cast<int>(adsrDecayMs)) + "ms");
        }
        
        paramDisplay7.setValue(adsrSustain);
        paramDisplay7.setValueText(juce::String(static_cast<int>(adsrSustain * 100.0f)) + "%");
        
        float releaseValue = adsrReleaseMs / 20000.0f;
        paramDisplay8.setValue(releaseValue);
        if (adsrReleaseMs >= 1000.0f) {
            paramDisplay8.setValueText(juce::String(adsrReleaseMs / 1000.0f, 2) + "s");
        } else {
            paramDisplay8.setValueText(juce::String(static_cast<int>(adsrReleaseMs)) + "ms");
        }
    }
}

void Op1CloneAudioProcessorEditor::buttonClicked(juce::Button* button) {
    if (button == &warpToggleButton) {
        bool enabled = warpToggleButton.getToggleState();
        audioProcessor.setTimeWarpEnabled(enabled);
    } else if (button == &shiftToggleButton) {
        // Shift button toggled - show/hide ADSR label
        bool shiftEnabled = shiftToggleButton.getToggleState();
        adsrLabel.setVisible(shiftEnabled);
        
        // BPM display is always visible, no need to toggle
        
        // ADSR visualization is now controlled by drag events, not shift button
        // Force repaint to show/hide components
        repaint();
    }
}

void Op1CloneAudioProcessorEditor::updateADSR()
{
    // Update visualization (only if explicitly being dragged/interacted with)
    // Don't update visualization on startup or when hidden
    if (isADSRDragging) {
        adsrVisualization.setADSR(adsrAttackMs, adsrDecayMs, adsrSustain, adsrReleaseMs);
    }
    
    // Send to processor (always update the audio processor)
    audioProcessor.setADSR(adsrAttackMs, adsrDecayMs, adsrSustain, adsrReleaseMs);
}

void Op1CloneAudioProcessorEditor::updateParameterDisplay(const juce::String& paramName, float value) {
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
    currentParameterText = paramName + " " + valueText;
    parameterDisplayLabel.setText(currentParameterText, juce::dontSendNotification);
    
    // Reset fade-out timer
    lastEncoderChangeTime = juce::Time::currentTimeMillis();
    parameterDisplayAlpha = 1.0f;
    parameterDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    parameterDisplayLabel.setVisible(true);
    parameterDisplayLabel.repaint();
}

void Op1CloneAudioProcessorEditor::updateSampleEditing() {
    // Send sample editing parameters to processor
    audioProcessor.setRepitch(repitchSemitones);
    audioProcessor.setStartPoint(startPoint);
    audioProcessor.setEndPoint(endPoint);
    audioProcessor.setSampleGain(sampleGain);
}

void Op1CloneAudioProcessorEditor::updateWaveformVisualization() {
    // Update waveform component with current start/end points and gain
    screenComponent.setStartPoint(startPoint);
    screenComponent.setEndPoint(endPoint);
    screenComponent.setSampleGain(sampleGain);
    // Force repaint to show the zoomed waveform
    screenComponent.repaint();
}

void Op1CloneAudioProcessorEditor::updateBPMDisplay() {
    // Show BPM value in top right corner (e.g., "BPM: 120")
    juce::String bpmText = "BPM: " + juce::String(projectBPM);
    bpmDisplayLabel.setText(bpmText, juce::dontSendNotification);
    bpmDisplayLabel.repaint();
}

