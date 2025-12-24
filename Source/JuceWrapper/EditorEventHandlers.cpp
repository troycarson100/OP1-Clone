#include "EditorEventHandlers.h"
#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>

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
            if (ed->audioProcessor.loadSampleFromFile(selectedFile)) {
                // Update UI
                ed->currentSampleName = selectedFile.getFileName();
                ed->sampleNameLabel.setText(ed->currentSampleName, juce::dontSendNotification);
                
                // Update waveform visualization
                ed->updateWaveform();
                
                ed->repaint();
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

