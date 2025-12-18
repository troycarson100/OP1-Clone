#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ScreenComponent.h"
#include "EncoderComponent.h"
#include "MidiStatusComponent.h"
#include "ADSRVisualizationComponent.h"
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
    
    // ADSR visualization overlay
    ADSRVisualizationComponent adsrVisualization;
    juce::Label adsrLabel;  // "ADSR" text overlay in top right
    juce::Label parameterDisplayLabel;  // Parameter value display (e.g., "A 1s") in top left
    juce::Label gainDisplayLabel;  // Gain display (e.g., "Gain 1.5x") in top right
    
    // Parameter display component (label + progress bar)
    class ParameterDisplay : public juce::Component {
    public:
        ParameterDisplay() {
            label.setJustificationType(juce::Justification::centredLeft);
            label.setColour(juce::Label::textColourId, juce::Colours::white);
            label.setFont(11.5f);  // Slightly bigger font for title
            addAndMakeVisible(&label);
            
            valueLabel.setJustificationType(juce::Justification::centredRight);
            valueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            valueLabel.setFont(11.5f);  // Same size for value
            addAndMakeVisible(&valueLabel);
        }
        
        void setLabel(const juce::String& text) {
            label.setText(text, juce::dontSendNotification);
        }
        
        void setValue(float value) {
            currentValue = juce::jlimit(0.0f, 1.0f, value);
            repaint();
        }
        
        void setValueText(const juce::String& text) {
            valueLabel.setText(text, juce::dontSendNotification);
        }
        
        void paint(juce::Graphics& g) override {
            auto bounds = getLocalBounds();
            int labelHeight = 18;  // Height for title/value row (slightly bigger for text)
            auto sliderArea = bounds.removeFromBottom(bounds.getHeight() - labelHeight);
            
            // Draw slider track (dark gray)
            g.setColour(juce::Colour(0xFF333333));
            g.fillRect(sliderArea);
            
            // Draw slider fill (white) based on value
            int fillWidth = static_cast<int>(sliderArea.getWidth() * currentValue);
            if (fillWidth > 0) {
                g.setColour(juce::Colours::white);
                g.fillRect(sliderArea.removeFromLeft(fillWidth));
            }
        }
        
        void resized() override {
            auto bounds = getLocalBounds();
            auto labelArea = bounds.removeFromTop(18);  // Height for title/value
            label.setBounds(labelArea.removeFromLeft(labelArea.getWidth() / 2));  // Title on left
            valueLabel.setBounds(labelArea);  // Value on right
        }
        
    private:
        juce::Label label;  // Title label
        juce::Label valueLabel;  // Value label
        float currentValue = 0.0f;
    };
    
    // 8 parameter displays at bottom of screen
    ParameterDisplay paramDisplay1;  // Pitch
    ParameterDisplay paramDisplay2;  // Start
    ParameterDisplay paramDisplay3;  // End
    ParameterDisplay paramDisplay4;  // Gain
    ParameterDisplay paramDisplay5;  // Attack
    ParameterDisplay paramDisplay6;  // Decay
    ParameterDisplay paramDisplay7;  // Sustain
    ParameterDisplay paramDisplay8;  // Release
    
    // MIDI status display
    MidiStatusComponent midiStatusComponent;
    
    // Hardware-style encoders
    EncoderComponent encoder1;
    EncoderComponent encoder2;
    EncoderComponent encoder3;
    EncoderComponent encoder4;
    EncoderComponent encoder5;
    EncoderComponent encoder6;
    EncoderComponent encoder7;
    EncoderComponent encoder8;
    
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
    
    // ADSR parameters (for UI)
    float adsrAttackMs;
    float adsrDecayMs;
    float adsrSustain;
    float adsrReleaseMs;
    
    // Sample editing parameters (for UI)
    float repitchSemitones;
    int startPoint;
    int endPoint;
    float sampleGain;
    int sampleLength; // Store sample length for encoder mapping
    double sampleRate; // Store sample rate for time calculations
    
    // Parameter display fade-out tracking
    int64_t lastEncoderChangeTime;  // Time when encoder was last moved (milliseconds)
    float parameterDisplayAlpha;  // Current alpha for fade-out (0.0 to 1.0)
    juce::String currentParameterText;  // Current parameter text to display
    
    // ADSR visualization fade-out tracking
    bool isADSRDragging;  // True when any ADSR encoder (5-8) is being dragged
    int64_t adsrFadeOutStartTime;  // Time when fade-out started (milliseconds)
    static constexpr int64_t ADSR_FADE_OUT_DURATION_MS = 1000;  // 1 second fade-out
    
    // Update ADSR visualization and send to processor
    void updateADSR();
    
    // Update parameter display text
    void updateParameterDisplay(const juce::String& paramName, float valueMs);
    
    // Update sample editing parameters and send to processor
    void updateSampleEditing();
    
    // Update waveform visualization with current parameters
    void updateWaveformVisualization();
    
    // Update gain display in top right corner
    void updateGainDisplay();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Op1CloneAudioProcessorEditor)
};

