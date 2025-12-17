#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

Op1CloneAudioProcessorEditor::Op1CloneAudioProcessorEditor(Op1CloneAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    
    // Setup screen component
    addAndMakeVisible(&screenComponent);
    
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
    
    // Setup info label
    infoLabel.setText("Keyboard: A-K = C4-C5, Z-M = C3-B3 | Or use MIDI controller", juce::dontSendNotification);
    infoLabel.setJustificationType(juce::Justification::centred);
    infoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(&infoLabel);
    
    // Setup sample label
    sampleLabel.setText("Sample: Default (440Hz tone)", juce::dontSendNotification);
    sampleLabel.setJustificationType(juce::Justification::centred);
    sampleLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible(&sampleLabel);
    
    currentSampleName = "Default (440Hz tone)";
    
    // Enable keyboard focus for MIDI keyboard input
    setWantsKeyboardFocus(true);
    
    // Update waveform with default sample
    updateWaveform();
    
    setSize(600, 500);
}

Op1CloneAudioProcessorEditor::~Op1CloneAudioProcessorEditor() {
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
    
    // Screen component takes up most of the space (70% width, 60% height - reduced by 40%)
    auto screenBounds = bounds.removeFromLeft(bounds.getWidth() * 0.7);
    screenBounds.setHeight(static_cast<int>(screenBounds.getHeight() * 0.6)); // 40% reduction = 60% of original
    screenComponent.setBounds(screenBounds.reduced(10));
    
    // Controls on the right side
    auto controlArea = bounds.reduced(10);
    controlArea.removeFromTop(30);
    
    // Sample label at top
    sampleLabel.setBounds(controlArea.removeFromTop(25).reduced(5));
    
    // Center the gain slider
    auto sliderArea = controlArea.reduced(10);
    gainSlider.setBounds(sliderArea.removeFromTop(150).reduced(20));
    gainLabel.setBounds(sliderArea.removeFromTop(20));
    
    // Buttons
    auto buttonArea = sliderArea.removeFromTop(40).reduced(10, 5);
    loadSampleButton.setBounds(buttonArea.removeFromTop(buttonArea.getHeight() / 2).reduced(5, 2));
    testButton.setBounds(buttonArea.reduced(5, 2));
    
    // Info label
    infoLabel.setBounds(sliderArea.removeFromTop(40).reduced(10));
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

void Op1CloneAudioProcessorEditor::updateWaveform() {
    // Get sample data from processor and update screen
    std::vector<float> sampleData;
    audioProcessor.getSampleDataForVisualization(sampleData);
    screenComponent.setSampleData(sampleData);
}

