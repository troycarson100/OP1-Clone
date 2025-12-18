#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ScreenComponent.h"
#include "EncoderComponent.h"
#include "MidiStatusComponent.h"
#include <array>

// Simple JUCE editor with gain slider
class Op1CloneAudioProcessorEditor : public juce::AudioProcessorEditor, 
                                     public juce::Timer,
                                     public juce::Button::Listener {
public:
    Op1CloneAudioProcessorEditor(Op1CloneAudioProcessor&);
    ~Op1CloneAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;  // Update error status display
    
    // Keyboard input handling
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
    
    // Button::Listener
    void buttonClicked(juce::Button* button) override;

private:
    Op1CloneAudioProcessor& audioProcessor;
    
    ScreenComponent screenComponent;
    
    // MIDI status display
    MidiStatusComponent midiStatusComponent;
    
    // Hardware-style encoders
    EncoderComponent encoder1;
    EncoderComponent encoder2;
    EncoderComponent encoder3;
    EncoderComponent encoder4;
    
    juce::Slider gainSlider;
    juce::Label volumeLabel;  // "Volume" text under the knob
    juce::Label sampleNameLabel;  // Sample name displayed above screen
    juce::TextButton loadSampleButton;
    juce::ToggleButton warpToggleButton;
    juce::TextButton shiftToggleButton;  // Square toggle button for "shift"
    juce::Label infoLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    
    // File chooser must be kept alive during async operation
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    void loadSampleButtonClicked();
    
    // Keyboard to MIDI mapping
    int keyToMidiNote(int keyCode) const;
    void sendMidiNote(int note, float velocity, bool noteOn);
    
    // Update waveform visualization
    void updateWaveform();
    
    juce::String currentSampleName;
    
    // Track which keys are currently pressed
    std::array<bool, 128> pressedKeys{};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Op1CloneAudioProcessorEditor)
};

