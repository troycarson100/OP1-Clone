#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

Op1CloneAudioProcessorEditor::Op1CloneAudioProcessorEditor(Op1CloneAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    
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
    infoLabel.setText("Click Test button or play MIDI note 60 (C4)", juce::dontSendNotification);
    infoLabel.setJustificationType(juce::Justification::centred);
    infoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(&infoLabel);
    
    // Setup sample label
    sampleLabel.setText("Sample: Default (440Hz tone)", juce::dontSendNotification);
    sampleLabel.setJustificationType(juce::Justification::centred);
    sampleLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible(&sampleLabel);
    
    currentSampleName = "Default (440Hz tone)";
    
    setSize(400, 400);
}

Op1CloneAudioProcessorEditor::~Op1CloneAudioProcessorEditor() {
}

void Op1CloneAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("OP-1 Clone Sampler", getLocalBounds(), juce::Justification::centredTop, 1);
}

void Op1CloneAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();
    bounds.removeFromTop(30);
    
    // Sample label at top
    sampleLabel.setBounds(bounds.removeFromTop(25).reduced(20, 5));
    
    // Center the gain slider
    auto sliderArea = bounds.reduced(20);
    gainSlider.setBounds(sliderArea.removeFromTop(150).reduced(50));
    gainLabel.setBounds(sliderArea.removeFromTop(20));
    
    // Buttons
    auto buttonArea = sliderArea.removeFromTop(40).reduced(60, 10);
    loadSampleButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2).reduced(5, 0));
    testButton.setBounds(buttonArea.reduced(5, 0));
    
    // Info label
    infoLabel.setBounds(sliderArea.removeFromTop(30).reduced(20));
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

