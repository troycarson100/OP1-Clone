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
    
    // Setup label
    gainLabel.setText("Gain", juce::dontSendNotification);
    gainLabel.attachToComponent(&gainSlider, false);
    gainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(&gainLabel);
    
    // Setup test button
    testButton.setButtonText("Test (Note 60)");
    testButton.onClick = [this] { testButtonClicked(); };
    addAndMakeVisible(&testButton);
    
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
    
    // Info label removed
    
    // Setup sample label
    sampleLabel.setText("Sample: Default (440Hz tone)", juce::dontSendNotification);
    sampleLabel.setJustificationType(juce::Justification::centred);
    sampleLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible(&sampleLabel);
    
    // Setup error status label
    errorStatusLabel.setText("Status: OK", juce::dontSendNotification);
    errorStatusLabel.setJustificationType(juce::Justification::centred);
    errorStatusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
    addAndMakeVisible(&errorStatusLabel);
    
    // Setup debug label
    debugLabel.setText("Debug: inN=0 outN=0 prime=0 nonZero=0", juce::dontSendNotification);
    debugLabel.setJustificationType(juce::Justification::centred);
    debugLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(&debugLabel);
    
    currentSampleName = "Default (440Hz tone)";
    
    // Enable keyboard focus for MIDI keyboard input
    setWantsKeyboardFocus(true);
    
    // Start timer to update error status (every 100ms)
    startTimer(100);
    
    // Update waveform with default sample
    updateWaveform();
    
    // Setup encoders (no labels)
    encoder1.onValueChanged = [this](float value) {
        // Encoder 1 value changed
        DBG("Encoder 1: " << value);
    };
    encoder1.onButtonPressed = [this]() {
        // Encoder 1 button pressed
        DBG("Encoder 1 button pressed");
    };
    
    encoder2.onValueChanged = [this](float value) {
        // Encoder 2 value changed
        DBG("Encoder 2: " << value);
    };
    encoder2.onButtonPressed = [this]() {
        // Encoder 2 button pressed
        DBG("Encoder 2 button pressed");
    };
    
    encoder3.onValueChanged = [this](float value) {
        // Encoder 3 value changed
        DBG("Encoder 3: " << value);
    };
    encoder3.onButtonPressed = [this]() {
        // Encoder 3 button pressed
        DBG("Encoder 3 button pressed");
    };
    
    encoder4.onValueChanged = [this](float value) {
        // Encoder 4 value changed
        DBG("Encoder 4: " << value);
    };
    encoder4.onButtonPressed = [this]() {
        // Encoder 4 button pressed
        DBG("Encoder 4 button pressed");
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
    
    // Left side: Controls (sample import, volume, test button)
    auto leftArea = bounds.removeFromLeft(200).reduced(10);
    
    // Sample label at top
    sampleLabel.setBounds(leftArea.removeFromTop(25).reduced(5));
    
    // Error status label
    errorStatusLabel.setBounds(leftArea.removeFromTop(20).reduced(5));
    
    // Debug label
    debugLabel.setBounds(leftArea.removeFromTop(20).reduced(5));
    
    // Load sample button
    loadSampleButton.setBounds(leftArea.removeFromTop(35).reduced(5));
    
    // Time-warp toggle (larger, easier to click)
    warpToggleButton.setBounds(leftArea.removeFromTop(40).reduced(5));
    
    // Gain/Volume encoder
    auto gainArea = leftArea.removeFromTop(120);
    gainSlider.setBounds(gainArea.removeFromTop(100).reduced(10));
    gainLabel.setBounds(gainArea.reduced(5));
    
    // Test button
    testButton.setBounds(leftArea.removeFromTop(35).reduced(5));
    
    // Middle: Screen component (40% less wide)
    auto screenBounds = bounds.removeFromLeft(bounds.getWidth() * 0.65 * 0.6); // 65% * 60% = 39% of remaining (40% reduction)
    screenBounds.setHeight(static_cast<int>(screenBounds.getHeight() * 0.6)); // 40% reduction = 60% of original
    screenComponent.setBounds(screenBounds.reduced(10));
    
    // Right side: Encoders in a horizontal row (moved up and left, closer to screen)
    auto encoderArea = bounds.reduced(10);
    int encoderSize = 80; // Much smaller encoders
    int encoderSpacing = 15;
    int totalEncoderWidth = (encoderSize + encoderSpacing) * 4 - encoderSpacing;
    // Move left (closer to screen) - start closer to left edge instead of centering
    int startX = encoderArea.getX() + 20; // 20px from left edge instead of centered
    int centerY = encoderArea.getY() + encoderArea.getHeight() * 0.25; // Move up more (25% from top)
    
    encoder1.setBounds(startX, centerY - encoderSize / 2, encoderSize, encoderSize);
    encoder2.setBounds(startX + encoderSize + encoderSpacing, centerY - encoderSize / 2, encoderSize, encoderSize);
    encoder3.setBounds(startX + (encoderSize + encoderSpacing) * 2, centerY - encoderSize / 2, encoderSize, encoderSize);
    encoder4.setBounds(startX + (encoderSize + encoderSpacing) * 3, centerY - encoderSize / 2, encoderSize, encoderSize);
    
    // MIDI status at bottom
    auto statusBounds = getLocalBounds().removeFromBottom(25);
    midiStatusComponent.setBounds(statusBounds);
}

void Op1CloneAudioProcessorEditor::testButtonClicked() {
    // Trigger test note through processor
    audioProcessor.triggerTestNote();
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
                sampleLabel.setText("Sample: " + currentSampleName, juce::dontSendNotification);
                sampleLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
                
                // Update waveform visualization
                updateWaveform();
                
                repaint();
            } else {
                // Show error in label
                sampleLabel.setText("Error loading: " + selectedFile.getFileName(), juce::dontSendNotification);
                sampleLabel.setColour(juce::Label::textColourId, juce::Colours::red);
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
    // Update error status from audio thread (safe, atomic read)
    auto& errorStatus = Core::TimePitchErrorStatus::getInstance();
    Core::TimePitchError error = errorStatus.getError();
    
    juce::String statusText = "Status: " + juce::String(errorStatus.getErrorString());
    
    if (error != Core::TimePitchError::OK) {
        errorStatusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    } else {
        errorStatusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
    }
    
    errorStatusLabel.setText(statusText, juce::dontSendNotification);
    
    // Update debug info (safe, atomic read)
    int actualInN = audioProcessor.debugLastActualInN.load(std::memory_order_acquire);
    int outN = audioProcessor.debugLastOutN.load(std::memory_order_acquire);
    int primeRemaining = audioProcessor.debugLastPrimeRemaining.load(std::memory_order_acquire);
    int nonZeroCount = audioProcessor.debugLastNonZeroOutCount.load(std::memory_order_acquire);
    
    juce::String debugText = "Debug: inN=" + juce::String(actualInN) + 
                             " outN=" + juce::String(outN) + 
                             " prime=" + juce::String(primeRemaining) + 
                             " nonZero=" + juce::String(nonZeroCount);
    debugLabel.setText(debugText, juce::dontSendNotification);
}

void Op1CloneAudioProcessorEditor::updateWaveform() {
    // Get sample data from processor and update screen
    std::vector<float> sampleData;
    audioProcessor.getSampleDataForVisualization(sampleData);
    screenComponent.setSampleData(sampleData);
}

void Op1CloneAudioProcessorEditor::buttonClicked(juce::Button* button) {
    if (button == &warpToggleButton) {
        bool enabled = warpToggleButton.getToggleState();
        audioProcessor.setTimeWarpEnabled(enabled);
    }
}

