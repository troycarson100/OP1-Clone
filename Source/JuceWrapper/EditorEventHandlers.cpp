#include "EditorEventHandlers.h"
#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

EditorEventHandlers::EditorEventHandlers(Op1CloneAudioProcessorEditor* editor)
    : editor(editor)
{
}

bool EditorEventHandlers::handleKeyPressed(const juce::KeyPress& key) {
    if (!editor) return false;  // Safety check
    
    int note = keyToMidiNote(key.getKeyCode());
    if (note >= 0 && note < 128 && !editor->pressedKeys[note]) {
        editor->pressedKeys[note] = true;
        sendMidiNote(note, 1.0f, true);
        return true;
    }
    return false;
}

bool EditorEventHandlers::handleKeyStateChanged(bool /* isKeyDown */) {
    if (!editor) return false;  // Safety check
    
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
        
        if (!keyCurrentlyDown && note >= 0 && note < 128 && editor->pressedKeys[note]) {
            // Key was released
            editor->pressedKeys[note] = false;
            sendMidiNote(note, 0.0f, false);
        }
    }
    return true;
}

void EditorEventHandlers::handleButtonClicked(juce::Button* button) {
    // Instrument select button (squareButton1)
    if (button == &editor->squareButton1) {
        // Close orbit menu if open (only one menu at a time)
        if (editor->orbitMenuOpen) {
            editor->orbitMenuOpen = false;
            editor->screenComponent.showOrbitVisualization(false);
            // Restore UI elements when closing orbit menu
            editor->sampleNameLabel.setVisible(true);
            editor->bpmDisplayLabel.setVisible(true);
            editor->adsrPillComponent.setVisible(true);
            editor->updateParameterDisplayLabels();  // Restore normal labels
        }
        
        // Toggle instrument menu
        editor->instrumentMenuOpen = !editor->instrumentMenuOpen;
        editor->screenComponent.showInstrumentMenu(editor->instrumentMenuOpen);
        
        // Hide/show parameter displays when menu is open
        bool menuVisible = editor->instrumentMenuOpen;
        editor->paramDisplay1.setVisible(!menuVisible);
        editor->paramDisplay2.setVisible(!menuVisible);
        editor->paramDisplay3.setVisible(!menuVisible);
        editor->paramDisplay4.setVisible(!menuVisible);
        editor->paramDisplay5.setVisible(!menuVisible);
        editor->paramDisplay6.setVisible(!menuVisible);
        editor->paramDisplay7.setVisible(!menuVisible);
        editor->paramDisplay8.setVisible(!menuVisible);
        
        // Also hide sample name label and BPM display
        editor->sampleNameLabel.setVisible(!menuVisible);
        editor->bpmDisplayLabel.setVisible(!menuVisible);
        
        // Hide ADSR pill component
        editor->adsrPillComponent.setVisible(!menuVisible);
        
        editor->repaint();
        return;
    }
    
    // Button 2: Orbit menu (auto-switch to Orbit mode if needed)
    if (button == &editor->squareButton2) {
        // If not in Orbit mode, switch to it first
        if (editor->playbackMode != 2) {
            editor->playbackMode = 2;
            editor->audioProcessor.setPlaybackMode(2);
            // Update encoder 4 display if in shift mode
            if (editor->shiftToggleButton.getToggleState()) {
                editor->paramDisplay4.setValue(1.0f);
                editor->paramDisplay4.setValueText("Orbit");
            }
        }
        
        // Close instrument menu if open (only one menu at a time)
        if (editor->instrumentMenuOpen) {
            editor->instrumentMenuOpen = false;
            editor->screenComponent.showInstrumentMenu(false);
        }
        
        // Toggle orbit menu
        editor->orbitMenuOpen = !editor->orbitMenuOpen;
        editor->screenComponent.showOrbitVisualization(editor->orbitMenuOpen);
        
        // Parameter displays should be VISIBLE when orbit menu is open (they show orbit-specific labels)
        bool orbitVisible = editor->orbitMenuOpen;
        editor->paramDisplay1.setVisible(true);  // Always visible - shows orbit controls
        editor->paramDisplay2.setVisible(true);
        editor->paramDisplay3.setVisible(true);
        editor->paramDisplay4.setVisible(true);
        editor->paramDisplay5.setVisible(true);
        editor->paramDisplay6.setVisible(true);
        editor->paramDisplay7.setVisible(true);
        editor->paramDisplay8.setVisible(true);
        
        // Hide sample name label and BPM display when orbit menu is open
        editor->sampleNameLabel.setVisible(!orbitVisible);
        editor->bpmDisplayLabel.setVisible(!orbitVisible);
        editor->adsrPillComponent.setVisible(!orbitVisible);
        
        // Initialize orbit visualization with current slot data
        if (editor->orbitMenuOpen) {
            for (int i = 0; i < 4; ++i) {
                std::vector<float> leftChannel, rightChannel;
                editor->audioProcessor.getSlotStereoSampleDataForVisualization(i, leftChannel, rightChannel);
                // Use left channel for preview (or mono mix if stereo)
                std::vector<float> previewData = leftChannel;
                if (!rightChannel.empty() && rightChannel.size() == leftChannel.size()) {
                    for (size_t j = 0; j < previewData.size(); ++j) {
                        previewData[j] = (previewData[j] + rightChannel[j]) * 0.5f;
                    }
                }
                editor->screenComponent.setOrbitSlotPreview(i, previewData);
            }
            
            // Update parameter display labels for orbit mode
            editor->updateParameterDisplayLabels();
        } else {
            // Update parameter display labels when closing orbit menu
            editor->updateParameterDisplayLabels();
            
            // Restore UI elements when closing orbit menu
            editor->sampleNameLabel.setVisible(true);
            editor->bpmDisplayLabel.setVisible(true);
            editor->adsrPillComponent.setVisible(true);
        }
        
        editor->repaint();
        return;
    }
    
    // Time warp toggle - COMMENTED OUT
    /*
    if (button == &editor->warpToggleButton) {
        bool enabled = editor->warpToggleButton.getToggleState();
    } else */
    if (button == &editor->shiftToggleButton) {
        // Shift button toggled - hide ADSR label when shift is on
        bool shiftEnabled = editor->shiftToggleButton.getToggleState();
        editor->adsrLabel.setVisible(false);  // Always hide ADSR label (shift mode uses different parameters)
        
        // Keep filter/effects enabled regardless of shift state
        // Shift only changes encoder behavior, not filter processing
        // editor->audioProcessor.setFilterEffectsEnabled(!shiftEnabled);  // REMOVED - keep filters always enabled
        
        // Update parameter display labels based on shift state
        editor->updateParameterDisplayLabels();
        
        // Update shift mode display values
        editor->updateShiftModeDisplayValues();
        
        // BPM display is always visible, no need to toggle
        
        // ADSR visualization is now controlled by drag events, not shift button
        // Force repaint to show/hide components
        editor->repaint();
    }
}

void EditorEventHandlers::handleLoadSampleButtonClicked() {
    // Open file picker for WAV files
    // FileChooser must be stored as member to stay alive during async operation
    editor->fileChooser = std::make_unique<juce::FileChooser>("Select a WAV sample file...",
                                                              juce::File(),
                                                              "*.wav;*.aif;*.aiff");
    
    auto chooserFlags = juce::FileBrowserComponent::openMode | 
                        juce::FileBrowserComponent::canSelectFiles;
    
    // CRITICAL: Capture editor pointer directly, not [this]
    // EditorEventHandlers is a temporary object, so [this] would be a dangling pointer
    // when the async callback executes. Capturing editor directly is safe as long as
    // the PluginEditor still exists.
    Op1CloneAudioProcessorEditor* ed = editor;
    editor->fileChooser->launchAsync(chooserFlags, [ed](const juce::FileChooser& fc) {
        auto selectedFile = fc.getResult();
        
        if (selectedFile.existsAsFile() && ed != nullptr) {
            // Load the sample file directly to the current slot (don't use loadSampleFromFile which sets default)
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(selectedFile));
            if (reader != nullptr) {
                juce::AudioBuffer<float> sampleBuffer(static_cast<int>(reader->numChannels), 
                                                      static_cast<int>(reader->lengthInSamples));
                if (reader->read(&sampleBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true)) {
                    // Save to current slot only - this preserves all other slots
                    int slotIndex = ed->currentSlotIndex;  // Capture current slot index
                    ed->audioProcessor.setSampleForSlot(slotIndex, sampleBuffer, reader->sampleRate);
                    
                    // For slot 0, also update the engine's default sample for backward compatibility
                    // This ensures the default sample matches slot 0, but doesn't affect other slots
                    // NOTE: We do NOT call loadSampleFromFile() here because it would update slot 0
                    // even when loading to other slots. Instead, we only update slot 0's data above.
                    // The engine's default sample is only used as a fallback when no slots are loaded.
                    
                    // Update UI
                    ed->currentSampleName = selectedFile.getFileName();
                    
                    // Update sampleRate and sampleLength for the current slot
                    ed->sampleRate = reader->sampleRate;
                    ed->sampleLength = static_cast<int>(reader->lengthInSamples);
                    
                    // Update sample name label with current slot letter (A-E)
                    char slotLetter = static_cast<char>('A' + slotIndex);
                    // Truncate sample name if too long to prevent overlap with ADSR pill
                    juce::String fullText = juce::String::charToString(slotLetter) + ": " + ed->currentSampleName;
                    
                    // Get available width for sample name (calculate from current layout)
                    auto screenBounds = ed->screenComponent.getBounds();
                    int bpmLeftEdge = screenBounds.getX() + screenBounds.getWidth() - 100;
                    int pillWidth = 80;
                    int spacing = 5;
                    int pillX = bpmLeftEdge - pillWidth - spacing;
                    int availableWidth = (pillX - 5) - (screenBounds.getX() + 10);  // Available width for sample name
                    
                    // Truncate if needed
                    juce::Font font = ed->sampleNameLabel.getFont();
                    if (font.getStringWidth(fullText) > availableWidth) {
                        juce::String truncated = fullText;
                        while (font.getStringWidth(truncated + "...") > availableWidth && truncated.length() > 0) {
                            truncated = truncated.substring(0, truncated.length() - 1);
                        }
                        fullText = truncated + "...";
                    }
                    
                    ed->sampleNameLabel.setText(fullText, juce::dontSendNotification);
                    
                    // Save sample name to current slot
                    ed->saveCurrentStateToSlot(slotIndex);
                    
                    // Update waveform visualization with current slot's sample
                    // This will also update the slot preview for the current slot
                    // Use the captured slotIndex to ensure we update the correct slot
                    ed->updateWaveform(slotIndex);
                    
                    // Update all slot previews to ensure all slots show their waveforms
                    ed->updateAllSlotPreviews();
                    
                    ed->repaint();
                } else {
                    // Error reading file
                }
            } else {
                // Error loading - could show a message box or just continue
                ed->repaint();
            }
        }
        
        // Clear file chooser after use
        if (ed != nullptr) {
            ed->fileChooser.reset();
        }
    });
}

int EditorEventHandlers::keyToMidiNote(int keyCode) const {
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

void EditorEventHandlers::sendMidiNote(int note, float velocity, bool noteOn) {
    // Send MIDI message through processor
    if (editor) {
        editor->audioProcessor.sendMidiNote(note, velocity, noteOn);
    }
}

