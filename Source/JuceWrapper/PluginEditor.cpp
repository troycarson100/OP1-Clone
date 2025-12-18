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
    , encoder4("") {
    
    // Setup screen component
    addAndMakeVisible(&screenComponent);
    
    // Setup ADSR visualization (overlay on screen)
    adsrVisualization.setAlwaysOnTop(true);
    adsrVisualization.setVisible(false);  // Hidden by default, shown when shift is enabled
    addChildComponent(&adsrVisualization);  // Use addChildComponent so it starts hidden
    
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
    
    // Setup gain display label (overlay in top right of screen when adjusting encoder 4)
    gainDisplayLabel.setText("", juce::dontSendNotification);
    gainDisplayLabel.setJustificationType(juce::Justification::centredRight);
    gainDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    gainDisplayLabel.setAlwaysOnTop(true);
    gainDisplayLabel.setVisible(false);  // Hidden by default
    addChildComponent(&gainDisplayLabel);
    
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
    
    // Setup sample name label (overlay on bottom left of screen)
    sampleNameLabel.setText("Default (440Hz tone)", juce::dontSendNotification);
    sampleNameLabel.setJustificationType(juce::Justification::centredLeft);
    sampleNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(&sampleNameLabel);
    sampleNameLabel.setAlwaysOnTop(true);
    
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
    
    // Setup encoders - control ADSR when shift is enabled
    encoder1.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Encoder 1: Attack (0-10000ms = 0-10 seconds)
            adsrAttackMs = value * 10000.0f;
            updateADSR();
            // Update parameter display
            updateParameterDisplay("A", adsrAttackMs);
        } else {
            // Encoder 1: Repitch (-12 to +12 semitones)
            repitchSemitones = (value - 0.5f) * 24.0f; // Map 0-1 to -12 to +12
            updateSampleEditing();
            // Update parameter display (show in semitones)
            updateParameterDisplay("Pitch", repitchSemitones);
        }
    };
    encoder1.onButtonPressed = [this]() {
        // Encoder 1 button pressed
    };
    
    encoder2.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Encoder 2: Decay (0-20000ms = 0-20 seconds)
            adsrDecayMs = value * 20000.0f;
            updateADSR();
            // Update parameter display
            updateParameterDisplay("D", adsrDecayMs);
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
            }
        }
    };
    encoder2.onButtonPressed = [this]() {
        // Encoder 2 button pressed
    };
    
    encoder3.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Encoder 3: Sustain (0.0-1.0)
            adsrSustain = value;
            updateADSR();
            // Update parameter display (show as percentage)
            updateParameterDisplay("S", adsrSustain * 100.0f);  // Convert to percentage
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
            }
        }
    };
    encoder3.onButtonPressed = [this]() {
        // Encoder 3 button pressed
    };
    
    encoder4.onValueChanged = [this](float value) {
        if (shiftToggleButton.getToggleState()) {
            // Encoder 4: Release (0-20000ms = 0-20 seconds)
            adsrReleaseMs = value * 20000.0f;
            updateADSR();
            // Update parameter display
            updateParameterDisplay("R", adsrReleaseMs);
        } else {
            // Encoder 4: Sample gain (0.0 to 2.0)
            sampleGain = value * 2.0f;
            updateSampleEditing();
            updateWaveformVisualization();
            // Update gain display in top right
            updateGainDisplay();
        }
    };
    encoder4.onButtonPressed = [this]() {
        // Encoder 4 button pressed
    };
    
    // Make encoders visible
    addAndMakeVisible(&encoder1);
    addAndMakeVisible(&encoder2);
    addAndMakeVisible(&encoder3);
    addAndMakeVisible(&encoder4);
    
    setSize(1200, 500); // Double width
}

Op1CloneAudioProcessorEditor::~Op1CloneAudioProcessorEditor() {
    stopTimer();
}

void Op1CloneAudioProcessorEditor::paint(juce::Graphics& g) {
    // Dark background
    g.fillAll(juce::Colour(0xFF2A2A2A));
    
    // Title at top
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawFittedText("OP-1 Clone Sampler", getLocalBounds().removeFromTop(25), 
                     juce::Justification::centred, 1);
}

void Op1CloneAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();
    bounds.removeFromTop(25); // Title area
    
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
    
    // Middle: Screen component (40% less wide)
    auto screenArea = bounds.removeFromLeft(bounds.getWidth() * 0.65 * 0.6); // 65% * 60% = 39% of remaining (40% reduction)
    auto screenBounds = screenArea.removeFromTop(static_cast<int>(screenArea.getHeight() * 0.6)); // 40% reduction = 60% of original
    screenComponent.setBounds(screenBounds.reduced(10));
    
    // Sample name label (overlay on bottom left of screen)
    auto screenComponentBounds = screenComponent.getBounds();
    sampleNameLabel.setBounds(screenComponentBounds.getX() + 10, 
                              screenComponentBounds.getBottom() - 25, 
                              screenComponentBounds.getWidth() - 20, 
                              20);
    
    // ADSR visualization overlay (60% of screen height, centered)
    int adsrHeight = static_cast<int>(screenComponentBounds.getHeight() * 0.6f);
    int adsrY = screenComponentBounds.getY() + (screenComponentBounds.getHeight() - adsrHeight) / 2;
    adsrVisualization.setBounds(screenComponentBounds.getX(),
                                adsrY,
                                screenComponentBounds.getWidth(),
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
    
    // Gain display label (overlay in top right of screen)
    gainDisplayLabel.setBounds(screenComponentBounds.getX() + screenComponentBounds.getWidth() - 100,
                               screenComponentBounds.getY() + 5,
                               90,
                               20);
    
    // Under screen: Load sample button (directly below the screen)
    loadSampleButton.setBounds(screenArea.removeFromTop(40).reduced(10));
    
        // Right side: Encoders in a horizontal row (moved up and left, closer to screen)
        auto encoderArea = bounds.reduced(10);
        int encoderSize = 80; // Much smaller encoders
        int encoderSpacing = 15;
        int totalEncoderWidth = (encoderSize + encoderSpacing) * 4 - encoderSpacing;
        // Move left (closer to screen) - start closer to left edge instead of centering
        int startX = encoderArea.getX() + 20; // 20px from left edge instead of centered
        int centerY = encoderArea.getY() + encoderArea.getHeight() * 0.15; // Move up more (15% from top, was 25%)
        
        encoder1.setBounds(startX, centerY - encoderSize / 2, encoderSize, encoderSize);
        encoder2.setBounds(startX + encoderSize + encoderSpacing, centerY - encoderSize / 2, encoderSize, encoderSize);
        encoder3.setBounds(startX + (encoderSize + encoderSpacing) * 2, centerY - encoderSize / 2, encoderSize, encoderSize);
        encoder4.setBounds(startX + (encoderSize + encoderSpacing) * 3, centerY - encoderSize / 2, encoderSize, encoderSize);
    
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
}

void Op1CloneAudioProcessorEditor::updateWaveform() {
    // Get sample data from processor and update screen
    std::vector<float> sampleData;
    audioProcessor.getSampleDataForVisualization(sampleData);
    screenComponent.setSampleData(sampleData);
    
    // Update sample length for encoder mapping
    sampleLength = static_cast<int>(sampleData.size());
    
    // Initialize start/end points if needed (first time loading or if they're invalid)
    if (sampleLength > 0) {
        if (endPoint == 0 || endPoint > sampleLength) {
            endPoint = sampleLength;
        }
        if (startPoint >= endPoint) {
            startPoint = 0;
        }
        // Update the processor with initial values
        updateSampleEditing();
        updateWaveformVisualization();
    }
}

void Op1CloneAudioProcessorEditor::buttonClicked(juce::Button* button) {
    if (button == &warpToggleButton) {
        bool enabled = warpToggleButton.getToggleState();
        audioProcessor.setTimeWarpEnabled(enabled);
    } else if (button == &shiftToggleButton) {
        // Shift button toggled - show/hide ADSR controls
        bool shiftEnabled = shiftToggleButton.getToggleState();
        adsrVisualization.setVisible(shiftEnabled);
        adsrLabel.setVisible(shiftEnabled);
        
        // Hide gain display when shift is enabled to avoid overlap with ADSR label
        if (shiftEnabled) {
            gainDisplayLabel.setVisible(false);
            // Update ADSR visualization with current values
            updateADSR();
        }
        
        // Force repaint to show/hide components
        repaint();
    }
}

void Op1CloneAudioProcessorEditor::updateADSR()
{
    // Update visualization
    adsrVisualization.setADSR(adsrAttackMs, adsrDecayMs, adsrSustain, adsrReleaseMs);
    
    // Send to processor
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

void Op1CloneAudioProcessorEditor::updateGainDisplay() {
    // Don't show gain display when shift is enabled (ADSR label is shown instead)
    if (shiftToggleButton.getToggleState()) {
        gainDisplayLabel.setVisible(false);
        return;
    }
    
    // Show gain value in top right corner (e.g., "Gain 1.5x")
    juce::String gainText = "Gain " + juce::String(sampleGain, 2) + "x";
    gainDisplayLabel.setText(gainText, juce::dontSendNotification);
    
    // Reset fade-out timer
    lastEncoderChangeTime = juce::Time::currentTimeMillis();
    parameterDisplayAlpha = 1.0f;
    gainDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    gainDisplayLabel.setVisible(true);
    gainDisplayLabel.repaint();
}

