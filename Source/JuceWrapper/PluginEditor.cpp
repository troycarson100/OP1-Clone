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
#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>

Op1CloneAudioProcessorEditor::Op1CloneAudioProcessorEditor(Op1CloneAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
    , menuEncoder("Menu")
    , encoder1("")
    , encoder2("")
    , encoder3("")
    , encoder4("")
    , encoder5("")
    , encoder6("")
    , encoder7("")
    , encoder8("") {
    
    // Setup menu encoder (outside screen component, on the left)
    addAndMakeVisible(&menuEncoder);
    
    // Setup screen component
    addAndMakeVisible(&screenComponent);
    
    // OLD ADSR visualization (COMMENTED OUT - replaced with pill component)
    /*
    // Setup ADSR visualization (overlay on screen)
    // Don't add to component tree until it's needed - this prevents any painting on startup
    // NOTE: setAlwaysOnTop(true) will be called when component is actually shown
    adsrVisualization.setInterceptsMouseClicks(false, false);  // Don't intercept mouse clicks
    // CRITICAL: Disable painting FIRST before any other operations
    adsrVisualization.setPaintingEnabled(false);  // CRITICAL: Disable painting on startup
    // Ensure it's not in component tree on startup
    if (adsrVisualization.getParentComponent() != nullptr) {
        adsrVisualization.getParentComponent()->removeChildComponent(&adsrVisualization);
    }
    // Set visibility and alpha AFTER ensuring it's not in tree (to avoid repaint)
    adsrVisualization.setVisible(false);  // Start hidden
    adsrVisualization.setAlpha(0.0f);  // Start invisible
    // CRITICAL: Set bounds to zero to prevent any rendering
    adsrVisualization.setBounds(0, 0, 0, 0);
    // Don't add to component tree yet - will be added when first shown
    */
    
    // NEW: Pill-shaped ADSR visualization (always visible)
    adsrPillComponent.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(&adsrPillComponent);
    
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
    lpCutoffHz = 20000.0f;  // Start at 20kHz (fully open)
    lpResonance = 1.0f;
    lpEnvAmount = 0.0f;  // DEPRECATED - kept for future use
    lpDriveDb = 0.0f;  // Default to 0 dB
    isPolyphonic = true;  // Default to Poly/Stereo
    playbackMode = 0;  // Default to Stacked (0 = Stacked, 1 = Round Robin)
    roundRobinIndex = 0;  // Start at first slot for round robin
    loopStartPoint = 0;
    loopEndPoint = 0;
    loopEnabled = false;  // Default to OFF
    loopEnvAttack = 10.0f;
    loopEnvRelease = 100.0f;
    
    // Initialize instrument menu state
    instrumentMenuOpen = false;
    lastMenuEncoderValue = 0.5f;  // Start at middle
    
    // Initialize slot snapshots (5 slots A-E)
    slotSnapshots.resize(5);
    currentSlotIndex = 0;  // Start with slot A
    
    // Save current state to slot A (this saves editor's current state to slot A's snapshot)
    saveCurrentStateToSlot(0);
    
    // Initialize adapter's per-slot parameters from snapshots (each slot has its own snapshot with defaults)
    // This ensures each slot has independent parameters from the start
    for (int i = 0; i < 5; ++i) {
        const Core::SlotSnapshot& snapshot = slotSnapshots[i];
        audioProcessor.setSlotRepitch(i, snapshot.repitchSemitones);
        audioProcessor.setSlotStartPoint(i, snapshot.startPoint);
        audioProcessor.setSlotEndPoint(i, snapshot.endPoint);
        audioProcessor.setSlotSampleGain(i, snapshot.sampleGain);
        audioProcessor.setSlotADSR(i, snapshot.attackMs, snapshot.decayMs, snapshot.sustain, snapshot.releaseMs);
        audioProcessor.setSlotLoopEnabled(i, snapshot.loopEnabled);
        audioProcessor.setSlotLoopPoints(i, snapshot.loopStartPoint, snapshot.loopEndPoint);
    }
    
    // Setup menu encoder (on left side of screen component)
    setupMenuEncoder();
    
    // Setup instrument menu callback
    screenComponent.setInstrumentMenuCallback([this](const juce::String& instrument) {
        // Handle instrument selection
        if (instrument == "Sampler") {
            // Already using sampler - do nothing for now
        } else if (instrument == "JNO") {
            // Will implement Juno synth later
        }
        instrumentMenuOpen = false;
        screenComponent.showInstrumentMenu(false);
        
        // Show parameter displays again when menu closes
        paramDisplay1.setVisible(true);
        paramDisplay2.setVisible(true);
        paramDisplay3.setVisible(true);
        paramDisplay4.setVisible(true);
        paramDisplay5.setVisible(true);
        paramDisplay6.setVisible(true);
        paramDisplay7.setVisible(true);
        paramDisplay8.setVisible(true);
        
        // Show sample name label and BPM display
        sampleNameLabel.setVisible(true);
        bpmDisplayLabel.setVisible(true);
        
        // Show ADSR pill component
        adsrPillComponent.setVisible(true);
        
        repaint();
    });
    
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
    sampleNameLabel.setText("A: Default (440Hz tone)", juce::dontSendNotification);
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
    
    // Setup load sample button (hidden - functionality moved to menu encoder center button)
    loadSampleButton.setButtonText("");
    loadSampleButton.setVisible(false);
    loadSampleButton.onClick = [this] { loadSampleButtonClicked(); };
    addChildComponent(&loadSampleButton);  // Use addChildComponent since it's hidden
    
    // Time-warp toggle (larger, more visible) - COMMENTED OUT
    /*
    warpToggleButton.setButtonText("Time Warp");
    warpToggleButton.setToggleState(true, juce::dontSendNotification);
    warpToggleButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    warpToggleButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::lightgreen);
    warpToggleButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    // Use Button::Listener pattern for reliable state changes
    warpToggleButton.addListener(this);
    // Also set initial state
    addAndMakeVisible(&warpToggleButton);
    */
    
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
    // Button 1: Instrument select (piano icon) - styled like button 2
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
    adsrAttackMs = 800.0f;  // Default: 800ms
    adsrDecayMs = 0.0f;
    adsrSustain = 1.0f;
    adsrReleaseMs = 1000.0f;  // Default: 1000ms (1s)
    
    // ADSR visualization fade-out tracking (COMMENTED OUT - replaced with pill component)
    isADSRDragging = false;
    isResettingADSR = false;
    adsrFadeOutStartTime = 0;
    // adsrVisualization.setAlpha(0.0f);  // Start invisible (COMMENTED OUT)
    // adsrVisualization.setVisible(false);  // Start hidden (COMMENTED OUT)
    
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
    // Attack: 800ms = 0.08 normalized (800/10000)
    paramDisplay5.setValue(0.08f);
    paramDisplay5.setValueText("0.80s");
    // Decay: 0ms = 0.0 normalized
    paramDisplay6.setValue(0.0f);
    paramDisplay6.setValueText("0ms");
    // Sustain: 1.0 = 1.0 normalized
    paramDisplay7.setValue(1.0f);
    paramDisplay7.setValueText("100%");
    // Release: 1000ms = 0.05 normalized (1000/20000)
    paramDisplay8.setValue(0.05f);
    paramDisplay8.setValueText("1.00s");
    
    // Initialize encoder 5-8 to ADSR defaults
    // Note: setValue already uses dontSendNotification, so callbacks won't fire
    encoder5.setValue(0.08f);   // Attack default (800ms = 0.08 normalized)
    encoder6.setValue(0.0f);    // Decay default
    encoder7.setValue(1.0f);    // Sustain default
    encoder8.setValue(0.05f);   // Release default (1000ms = 0.05 normalized)
    
    // OLD ADSR visualization hiding code (COMMENTED OUT - replaced with pill component)
    /*
    // Explicitly ensure ADSR visualization is hidden after initialization
    // Remove from component tree if it was somehow added (do this FIRST)
    if (adsrVisualization.getParentComponent() != nullptr) {
        adsrVisualization.getParentComponent()->removeChildComponent(&adsrVisualization);
    }
    // Set visibility and alpha AFTER removal to avoid triggering repaint
    adsrVisualization.setVisible(false);
    adsrVisualization.setAlpha(0.0f);
    adsrVisualization.setPaintingEnabled(false);  // CRITICAL: Disable painting
    // CRITICAL: Set bounds to zero to prevent any painting
    adsrVisualization.setBounds(0, 0, 0, 0);
    */
    isADSRDragging = false;
    
    // Setup encoders using manager (extracted to comply with 500-line rule)
    EncoderSetupManager encoderSetupManager(this);
    encoderSetupManager.setupEncoders();
    
    // Initialize shift mode display values if shift is enabled (should be false by default, but ensure sync)
    // This ensures encoder7 (Loop) is set to 0.0f (OFF) if shift mode is enabled
    if (shiftToggleButton.getToggleState()) {
        updateShiftModeDisplayValues();
    }
    
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
    
    // OLD ADSR visualization removal code (COMMENTED OUT - replaced with pill component)
    /*
    // CRITICAL: Ensure ADSR visualization is completely disabled ONLY if not being dragged AND not fading
    // Don't interfere if user is actively interacting with ADSR encoders or if fade-out is in progress
    // This check happens on EVERY paint to catch any components that might have been added
    if (!isADSRDragging && adsrFadeOutStartTime == 0) {
        // Always ensure painting is disabled if not in use
        if (!adsrVisualization.isPaintingEnabled() && adsrVisualization.getParentComponent() != nullptr) {
            // Component is in tree but painting is disabled - remove it
            adsrVisualization.getParentComponent()->removeChildComponent(&adsrVisualization);
            adsrVisualization.setVisible(false);
            adsrVisualization.setAlpha(0.0f);
            adsrVisualization.setBounds(0, 0, 0, 0);
        } else if (adsrVisualization.getParentComponent() != nullptr && !adsrVisualization.isPaintingEnabled()) {
            // Component is in tree but shouldn't be - remove it
            adsrVisualization.getParentComponent()->removeChildComponent(&adsrVisualization);
            adsrVisualization.setPaintingEnabled(false);  // CRITICAL: Disable painting
            adsrVisualization.setVisible(false);
            adsrVisualization.setAlpha(0.0f);
            adsrVisualization.setBounds(0, 0, 0, 0);
        }
    }
    */
}

void Op1CloneAudioProcessorEditor::resized() {
    // OLD ADSR visualization removal code (COMMENTED OUT - replaced with pill component)
    /*
    // CRITICAL: Ensure ADSR visualization is NEVER in component tree on startup/resize
    // Only add it when user actually interacts with ADSR encoders
    // Remove it immediately if it's in the tree and not being actively used or fading
    if (!isADSRDragging && adsrFadeOutStartTime == 0) {
        if (adsrVisualization.getParentComponent() != nullptr) {
            adsrVisualization.getParentComponent()->removeChildComponent(&adsrVisualization);
        }
        // Set visibility and alpha AFTER removal to prevent any painting
        adsrVisualization.setVisible(false);
        adsrVisualization.setAlpha(0.0f);
        adsrVisualization.setPaintingEnabled(false);  // CRITICAL: Disable painting
        // CRITICAL: Set bounds to zero to prevent any painting
        adsrVisualization.setBounds(0, 0, 0, 0);
    }
    */
    
    // Delegate layout to manager (extracted to comply with 500-line rule)
    EditorLayoutManager layoutManager(this);
    layoutManager.layoutComponents();
    
    // Initialize waveform on first resize (after window is shown)
    if (!waveformInitialized) {
        waveformInitialized = true;
        // Try to load the waveform - if sample isn't loaded yet, it will be retried in timerCallback
        // Use a delayed call to ensure sample data is loaded
        juce::MessageManager::callAsync([this]() {
            updateWaveform(currentSlotIndex);
            updateWaveformVisualization();
            updateAllSlotPreviews();
            repaint();
        });
    }
    
    // OLD final safety check (COMMENTED OUT - replaced with pill component)
    /*
    // Final safety check: ensure ADSR visualization is removed if not being used
    // But don't interfere if dragging or fading
    if (!isADSRDragging && adsrFadeOutStartTime == 0 && adsrVisualization.getParentComponent() != nullptr) {
        adsrVisualization.getParentComponent()->removeChildComponent(&adsrVisualization);
        adsrVisualization.setVisible(false);
        adsrVisualization.setAlpha(0.0f);
        adsrVisualization.setPaintingEnabled(false);  // CRITICAL: Disable painting
        adsrVisualization.setBounds(0, 0, 0, 0);
    }
    */
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

void Op1CloneAudioProcessorEditor::updateWaveform(int slotIndex) {
    // Delegate to update methods manager with specific slot index
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateWaveform(slotIndex);
}

void Op1CloneAudioProcessorEditor::updateAllSlotPreviews() {
    // Delegate to update methods manager
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateAllSlotPreviews();
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

// OLD setADSRVisualizationBounds (COMMENTED OUT - replaced with pill component)
/*
void Op1CloneAudioProcessorEditor::setADSRVisualizationBounds() {
    // Only set bounds if component is actually in the tree and being used
    if (adsrVisualization.getParentComponent() == nullptr || !isADSRDragging) {
        return;  // Don't set bounds if not in tree or not being used
    }
    // Set bounds for ADSR visualization based on waveform bounds
    auto waveformBounds = screenComponent.getWaveformBounds();
    auto screenBounds = screenComponent.getBounds();
    juce::Rectangle<int> waveformBoundsInEditor(
        screenBounds.getX() + waveformBounds.getX(),
        screenBounds.getY() + waveformBounds.getY(),
        waveformBounds.getWidth(),
        waveformBounds.getHeight()
    );
    int adsrHeight = static_cast<int>(waveformBoundsInEditor.getHeight() * 0.4f);
    int adsrY = waveformBoundsInEditor.getCentreY() - adsrHeight / 2;
    adsrVisualization.setBounds(waveformBoundsInEditor.getX(),
                                adsrY,
                                waveformBoundsInEditor.getWidth(),
                                adsrHeight);
}
*/

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

void Op1CloneAudioProcessorEditor::updateShiftModeDisplayValues() {
    // Delegate to update methods manager (extracted to comply with 500-line rule)
    EditorUpdateMethods updateMethods(this);
    updateMethods.updateShiftModeDisplayValues();
}

void Op1CloneAudioProcessorEditor::setupMenuEncoder() {
    // Setup menu encoder for instrument menu navigation and slot selection
    menuEncoder.onValueChanged = [this](float value) {
        // Check if instrument menu is open - if so, handle navigation
        if (instrumentMenuOpen) {
            // Calculate direction of change
            float delta = value - lastMenuEncoderValue;
            
            // Normalize delta to determine direction (handle wrap-around)
            if (delta > 0.5f) delta -= 1.0f;  // Wrapped from high to low
            if (delta < -0.5f) delta += 1.0f;  // Wrapped from low to high
            
            // Only respond to significant changes
            if (std::abs(delta) > 0.01f) {
                int currentIndex = screenComponent.getInstrumentMenuSelectedIndex();
                int numInstruments = 2;  // Sampler, JNO
                
                if (delta > 0) {
                    // Scrolled up - move selection down
                    currentIndex = (currentIndex + 1) % numInstruments;
                } else {
                    // Scrolled down - move selection up
                    currentIndex = (currentIndex - 1 + numInstruments) % numInstruments;
                }
                
                screenComponent.setInstrumentMenuSelectedIndex(currentIndex);
                repaint();
            }
            
            lastMenuEncoderValue = value;
            return;  // Don't process slot selection when menu is open
        }
        
        // Not in menu - handle slot selection (A-E)
        // Use smoother mapping: map encoder value (0-1) directly to slot index (0-4)
        // This provides continuous, smooth rotation through slots
        float slotValue = value * 5.0f;  // Map 0-1 to 0-5
        int newSlot = static_cast<int>(slotValue) % 5;
        
        // Only change slot if it's different from current
        if (newSlot != currentSlotIndex) {
            loadStateFromSlot(newSlot);
        }
        
        lastMenuEncoderValue = value;
    };
    
    menuEncoder.onButtonPressed = [this]() {
        if (instrumentMenuOpen) {
            // Select instrument when menu is open
            screenComponent.selectInstrument();
        } else {
            // When instrument menu is closed, center button loads sample for current slot
            loadSampleButtonClicked();
        }
    };
}

void Op1CloneAudioProcessorEditor::saveCurrentStateToSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= 5) return;
    
    Core::SlotSnapshot& snapshot = slotSnapshots[slotIndex];
    snapshot.repitchSemitones = repitchSemitones;
    snapshot.startPoint = startPoint;
    snapshot.endPoint = endPoint;
    snapshot.sampleGain = sampleGain;
    snapshot.lpCutoffHz = lpCutoffHz;
    snapshot.lpResonance = lpResonance;
    snapshot.lpDriveDb = lpDriveDb;
    snapshot.attackMs = adsrAttackMs;
    snapshot.decayMs = adsrDecayMs;
    snapshot.sustain = adsrSustain;
    snapshot.releaseMs = adsrReleaseMs;
    snapshot.loopStartPoint = loopStartPoint;
    snapshot.loopEndPoint = loopEndPoint;
    snapshot.loopEnabled = loopEnabled;
    snapshot.isPolyphonic = isPolyphonic;
    snapshot.playbackMode = playbackMode;
    snapshot.sampleName = currentSampleName.toStdString();  // Save sample name
}

void Op1CloneAudioProcessorEditor::loadStateFromSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= 5) return;
    
    // Save current state before switching (save editor's current state to the current slot's snapshot)
    saveCurrentStateToSlot(currentSlotIndex);
    
    // Also sync adapter's per-slot parameters from editor's current state before switching
    audioProcessor.setSlotRepitch(currentSlotIndex, repitchSemitones);
    audioProcessor.setSlotStartPoint(currentSlotIndex, startPoint);
    audioProcessor.setSlotEndPoint(currentSlotIndex, endPoint);
    audioProcessor.setSlotSampleGain(currentSlotIndex, sampleGain);
    audioProcessor.setSlotADSR(currentSlotIndex, adsrAttackMs, adsrDecayMs, adsrSustain, adsrReleaseMs);
    audioProcessor.setSlotLoopEnabled(currentSlotIndex, loopEnabled);
    audioProcessor.setSlotLoopPoints(currentSlotIndex, loopStartPoint, loopEndPoint);
    
    // Load new slot
    currentSlotIndex = slotIndex;
    const Core::SlotSnapshot& snapshot = slotSnapshots[slotIndex];
    repitchSemitones = snapshot.repitchSemitones;
    startPoint = snapshot.startPoint;
    endPoint = snapshot.endPoint;
    sampleGain = snapshot.sampleGain;
    lpCutoffHz = snapshot.lpCutoffHz;
    lpResonance = snapshot.lpResonance;
    lpDriveDb = snapshot.lpDriveDb;
    adsrAttackMs = snapshot.attackMs;
    adsrDecayMs = snapshot.decayMs;
    adsrSustain = snapshot.sustain;
    adsrReleaseMs = snapshot.releaseMs;
    loopStartPoint = snapshot.loopStartPoint;
    loopEndPoint = snapshot.loopEndPoint;
    loopEnabled = snapshot.loopEnabled;
    isPolyphonic = snapshot.isPolyphonic;
    playbackMode = snapshot.playbackMode;
    currentSampleName = juce::String(snapshot.sampleName);  // Load sample name
    
    // Update sample name label with slot letter (A-E)
    char slotLetter = 'A' + slotIndex;
    sampleNameLabel.setText(juce::String::charToString(slotLetter) + ": " + currentSampleName, juce::dontSendNotification);
    
    // Update sampleRate and sampleLength for the current slot
    sampleRate = audioProcessor.getSlotSourceSampleRate(slotIndex);
    std::vector<float> leftChannel, rightChannel;
    audioProcessor.getSlotStereoSampleDataForVisualization(slotIndex, leftChannel, rightChannel);
    if (!leftChannel.empty()) {
        sampleLength = static_cast<int>(leftChannel.size());
    } else {
        sampleLength = 0;
    }
    
    // Sync adapter's per-slot parameters with loaded snapshot (ensure adapter has this slot's parameters)
    audioProcessor.setSlotRepitch(slotIndex, repitchSemitones);
    audioProcessor.setSlotStartPoint(slotIndex, startPoint);
    audioProcessor.setSlotEndPoint(slotIndex, endPoint);
    audioProcessor.setSlotSampleGain(slotIndex, sampleGain);
    audioProcessor.setSlotADSR(slotIndex, adsrAttackMs, adsrDecayMs, adsrSustain, adsrReleaseMs);
    audioProcessor.setSlotLoopEnabled(slotIndex, loopEnabled);
    audioProcessor.setSlotLoopPoints(slotIndex, loopStartPoint, loopEndPoint);
    
    // Apply values to processor and UI
    updateSampleEditing();
    updateADSR();
    updateShiftModeDisplayValues();
    
    // Update waveform for the loaded slot (this also updates the visualization)
    updateWaveform(slotIndex);
    updateWaveformVisualization();
    
    // Update all slot previews to ensure all slots show their waveforms correctly
    updateAllSlotPreviews();
    
    // Update parameter display labels to reflect loaded values
    updateParameterDisplayLabels();
    
    // Sync encoder values with loaded parameters (so encoders show correct position)
    syncEncodersWithCurrentValues();
    
    // Update screen component to show selected slot
    screenComponent.setSelectedSlot(slotIndex);
}

void Op1CloneAudioProcessorEditor::syncEncodersWithCurrentValues() {
    // Sync encoder positions with current parameter values
    // This ensures encoders show the correct position when loading a slot
    
    if (shiftToggleButton.getToggleState()) {
        // Shift mode: Map filter parameters to encoder positions
        // Encoder 1: LP Filter Cutoff (20-20000 Hz, logarithmic)
        float cutoffNormalized = std::log(lpCutoffHz / 20.0f) / std::log(1000.0f);
        encoder1.setValue(cutoffNormalized);
        
        // Encoder 2: LP Filter Resonance (0.0-4.0)
        encoder2.setValue(lpResonance / 4.0f);
        
        // Encoder 3: Drive (0.0-24.0 dB)
        encoder3.setValue(lpDriveDb / 24.0f);
        
        // Encoder 4: No function in shift mode
        
        // Encoder 5: Loop Start Point
        if (sampleLength > 0) {
            encoder5.setValue(static_cast<float>(loopStartPoint) / static_cast<float>(sampleLength));
        }
        
        // Encoder 6: Loop End Point
        if (sampleLength > 0) {
            encoder6.setValue(static_cast<float>(loopEndPoint) / static_cast<float>(sampleLength));
        }
        
        // Encoder 7: Loop (on/off)
        encoder7.setValue(loopEnabled ? 1.0f : 0.0f);
        
        // Encoder 8: Playback (Mono/Poly)
        encoder8.setValue(isPolyphonic ? 1.0f : 0.0f);
    } else {
        // Normal mode: Map sample editing and ADSR parameters
        // Encoder 1: Repitch (-24 to +24 semitones)
        encoder1.setValue((repitchSemitones + 24.0f) / 48.0f);
        
        // Encoder 2: Start point
        if (sampleLength > 0) {
            encoder2.setValue(static_cast<float>(startPoint) / static_cast<float>(sampleLength));
        }
        
        // Encoder 3: End point
        if (sampleLength > 0) {
            encoder3.setValue(static_cast<float>(endPoint) / static_cast<float>(sampleLength));
        }
        
        // Encoder 4: Sample gain (0.0-2.0)
        encoder4.setValue(sampleGain / 2.0f);
        
        // Encoder 5: Attack (0-10000ms)
        encoder5.setValue(adsrAttackMs / 10000.0f);
        
        // Encoder 6: Decay (0-20000ms)
        encoder6.setValue(adsrDecayMs / 20000.0f);
        
        // Encoder 7: Sustain (0.0-1.0)
        encoder7.setValue(adsrSustain);
        
        // Encoder 8: Release (0-20000ms)
        encoder8.setValue(adsrReleaseMs / 20000.0f);
    }
}

