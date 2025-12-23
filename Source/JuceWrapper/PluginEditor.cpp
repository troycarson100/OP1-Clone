#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EncoderSetupManager.h"
#include "EditorLayoutManager.h"
#include "EditorEventHandlers.h"
#include "EditorUpdateMethods.h"
#include "EditorTimerCallback.h"
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
    waveformInitialized = false;
    
    // Initialize shift mode parameters
    lpCutoffHz = 10000.0f;  // Start at 10kHz so filter is active
    lpResonance = 1.0f;
    lpEnvAmount = 0.0f;  // DEPRECATED - kept for future use
    lpDriveDb = 0.0f;
    timeWarpSpeed = 1.0f;  // Default to normal speed (1.0x)
    isPolyphonic = true;
    loopStartPoint = 0;
    loopEndPoint = 0;
    loopEnabled = false;
    loopEnvAttack = 10.0f;
    loopEnvRelease = 100.0f;
    
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
    
    // Setup 5 square buttons above encoders
    squareButton1.setButtonText("");
    squareButton1.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    squareButton1.addListener(this);
    addAndMakeVisible(&squareButton1);
    
    squareButton2.setButtonText("");
    squareButton2.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    squareButton2.addListener(this);
    addAndMakeVisible(&squareButton2);
    
    squareButton3.setButtonText("");
    squareButton3.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    squareButton3.addListener(this);
    addAndMakeVisible(&squareButton3);
    
    squareButton4.setButtonText("");
    squareButton4.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    squareButton4.addListener(this);
    addAndMakeVisible(&squareButton4);
    
    squareButton5.setButtonText("");
    squareButton5.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    squareButton5.addListener(this);
    addAndMakeVisible(&squareButton5);
    
    // Info label removed
    
    currentSampleName = "Default (440Hz tone)";
    
    // Enable keyboard focus for MIDI keyboard input
    setWantsKeyboardFocus(true);
    
    // Start timer to update error status (every 100ms)
    startTimer(100);
    
    // ADSR parameters (in milliseconds, except sustain which is 0.0-1.0)
    adsrAttackMs = 2.0f;
    adsrDecayMs = 0.0f;
    adsrSustain = 1.0f;
    adsrReleaseMs = 20.0f;
    
    // ADSR visualization fade-out tracking
    isADSRDragging = false;
    isResettingADSR = false;
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
    
    // Setup encoders using manager (extracted to comply with 500-line rule)
    EncoderSetupManager encoderSetupManager(this);
    encoderSetupManager.setupEncoders();
    
    // Don't call updateWaveform() here - it will be called from resized() after window is shown
    
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
    // Delegate layout to manager (extracted to comply with 500-line rule)
    EditorLayoutManager layoutManager(this);
    layoutManager.layoutComponents();
    
    // Initialize waveform on first resize (after window is shown)
    if (!waveformInitialized) {
        waveformInitialized = true;
        updateWaveform();
    }
}

void Op1CloneAudioProcessorEditor::loadSampleButtonClicked() {
    // Delegate to event handlers manager (extracted to comply with 500-line rule)
    EditorEventHandlers eventHandlers(this);
    eventHandlers.handleLoadSampleButtonClicked();
}

bool Op1CloneAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    // Delegate to event handlers manager (extracted to comply with 500-line rule)
    EditorEventHandlers eventHandlers(this);
    return eventHandlers.handleKeyPressed(key);
}

bool Op1CloneAudioProcessorEditor::keyStateChanged(bool isKeyDown) {
    // Delegate to event handlers manager (extracted to comply with 500-line rule)
    EditorEventHandlers eventHandlers(this);
    return eventHandlers.handleKeyStateChanged(isKeyDown);
}

int Op1CloneAudioProcessorEditor::keyToMidiNote(int keyCode) const {
    // Delegate to event handlers manager (extracted to comply with 500-line rule)
    EditorEventHandlers eventHandlers(const_cast<Op1CloneAudioProcessorEditor*>(this));
    return eventHandlers.keyToMidiNote(keyCode);
}

void Op1CloneAudioProcessorEditor::sendMidiNote(int note, float velocity, bool noteOn) {
    // Delegate to event handlers manager (extracted to comply with 500-line rule)
    EditorEventHandlers eventHandlers(this);
    eventHandlers.sendMidiNote(note, velocity, noteOn);
}

void Op1CloneAudioProcessorEditor::timerCallback() {
    // Delegate to timer callback manager (extracted to comply with 500-line rule)
    EditorTimerCallback timerHandler(this);
    timerHandler.handleTimerCallback();
}

void Op1CloneAudioProcessorEditor::updateWaveform() {
    // Delegate to update methods manager (extracted to comply with 500-line rule)
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateWaveform();
}

void Op1CloneAudioProcessorEditor::buttonClicked(juce::Button* button) {
    // Delegate to event handlers manager (extracted to comply with 500-line rule)
    EditorEventHandlers eventHandlers(this);
    eventHandlers.handleButtonClicked(button);
}

void Op1CloneAudioProcessorEditor::updateADSR() {
    // Delegate to update methods manager (extracted to comply with 500-line rule)
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateADSR();
}

void Op1CloneAudioProcessorEditor::updateParameterDisplay(const juce::String& paramName, float value) {
    // Delegate to update methods manager (extracted to comply with 500-line rule)
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateParameterDisplay(paramName, value);
}

void Op1CloneAudioProcessorEditor::updateSampleEditing() {
    // Delegate to update methods manager (extracted to comply with 500-line rule)
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateSampleEditing();
}

void Op1CloneAudioProcessorEditor::updateWaveformVisualization() {
    // Delegate to update methods manager (extracted to comply with 500-line rule)
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateWaveformVisualization();
}

void Op1CloneAudioProcessorEditor::updateBPMDisplay() {
    // Delegate to update methods manager (extracted to comply with 500-line rule)
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateBPMDisplay();
}

void Op1CloneAudioProcessorEditor::updateParameterDisplayLabels() {
    // Delegate to update methods manager (extracted to comply with 500-line rule)
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateParameterDisplayLabels();
}

void Op1CloneAudioProcessorEditor::updateLoopControlsState() {
    // Delegate to update methods manager (extracted to comply with 500-line rule)
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateLoopControlsState();
}

