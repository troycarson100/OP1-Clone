#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Simple JUCE editor with gain slider
class Op1CloneAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    Op1CloneAudioProcessorEditor(Op1CloneAudioProcessor&);
    ~Op1CloneAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    Op1CloneAudioProcessor& audioProcessor;
    
    juce::Slider gainSlider;
    juce::Label gainLabel;
    juce::TextButton testButton;
    juce::TextButton loadSampleButton;
    juce::Label infoLabel;
    juce::Label sampleLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    
    // File chooser must be kept alive during async operation
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    void testButtonClicked();
    void loadSampleButtonClicked();
    
    juce::String currentSampleName;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Op1CloneAudioProcessorEditor)
};

