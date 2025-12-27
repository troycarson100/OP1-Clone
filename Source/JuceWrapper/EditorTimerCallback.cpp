#include "EditorTimerCallback.h"
#include "PluginEditor.h"
#include <juce_core/juce_core.h>

EditorTimerCallback::EditorTimerCallback(Op1CloneAudioProcessorEditor* editor)
    : editor(editor)
{
}

void EditorTimerCallback::handleTimerCallback() {
    // Handle parameter display fade-out
    if (editor->parameterDisplayAlpha > 0.0f) {
        int64_t currentTime = juce::Time::currentTimeMillis();
        int64_t timeSinceLastChange = currentTime - editor->lastEncoderChangeTime;
        
        // If 1 second (1000ms) has passed since last encoder change, start fading out
        if (timeSinceLastChange > 1000) {
            // Fade out over 1 second (100ms timer = 10 steps)
            editor->parameterDisplayAlpha -= 0.1f;
            if (editor->parameterDisplayAlpha < 0.0f) {
                editor->parameterDisplayAlpha = 0.0f;
                editor->parameterDisplayLabel.setVisible(false);
            } else {
                // Update label color with alpha
                editor->parameterDisplayLabel.setColour(juce::Label::textColourId, 
                    juce::Colours::white.withAlpha(editor->parameterDisplayAlpha));
                editor->parameterDisplayLabel.repaint();
            }
        }
    }
    
    // OLD ADSR visualization fade-out code (COMMENTED OUT - replaced with pill component)
    // Pill component stays visible, no fade-out needed
    /*
    // Handle ADSR visualization fade-out
    // Only process fade-out if component is in tree and being used
    if (!editor->isADSRDragging && editor->adsrFadeOutStartTime > 0) {
        // Check if component is in tree - if not, remove it immediately
        if (editor->adsrVisualization.getParentComponent() == nullptr) {
            editor->adsrFadeOutStartTime = 0;  // Reset fade timer
            return;  // Component not in tree, nothing to fade
        }
        
        if (editor->adsrVisualization.isVisible()) {
            int64_t currentTime = juce::Time::currentTimeMillis();
            int64_t elapsed = currentTime - editor->adsrFadeOutStartTime;
            const int64_t ADSR_FADE_OUT_DURATION_MS = 1000;  // 1 second fade-out
            
            if (elapsed >= ADSR_FADE_OUT_DURATION_MS) {
                // Fade complete - hide and remove from tree
                editor->adsrVisualization.setPaintingEnabled(false);  // Disable painting
                editor->adsrVisualization.setAlpha(0.0f);
                editor->adsrVisualization.setVisible(false);
                if (editor->adsrVisualization.getParentComponent() != nullptr) {
                    editor->adsrVisualization.getParentComponent()->removeChildComponent(&editor->adsrVisualization);
                }
                editor->adsrVisualization.setBounds(0, 0, 0, 0);  // Set bounds to zero
                editor->adsrFadeOutStartTime = 0;
            } else {
                // Calculate fade progress (0.0 to 1.0)
                float fadeProgress = static_cast<float>(elapsed) / static_cast<float>(ADSR_FADE_OUT_DURATION_MS);
                float alpha = 1.0f - fadeProgress;
                editor->adsrVisualization.setAlpha(alpha);
                editor->adsrVisualization.repaint();
            }
        }
    } else if (!editor->isADSRDragging && editor->adsrFadeOutStartTime == 0) {
        // Not dragging and no fade-out active - ensure component is removed from tree
        if (editor->adsrVisualization.getParentComponent() != nullptr) {
            editor->adsrVisualization.getParentComponent()->removeChildComponent(&editor->adsrVisualization);
            editor->adsrVisualization.setPaintingEnabled(false);  // Disable painting
            editor->adsrVisualization.setVisible(false);
            editor->adsrVisualization.setAlpha(0.0f);
            editor->adsrVisualization.setBounds(0, 0, 0, 0);  // Set bounds to zero
        }
    }
    */
    
    // Update active slots display (which slots are currently playing)
    std::array<bool, 5> activeSlots = editor->audioProcessor.getActiveSlots();
    editor->screenComponent.setActiveSlots(activeSlots);
    
    // Update orbit visualization if in Orbit mode and menu is open
    if (editor->playbackMode == 2 && editor->orbitMenuOpen) {
        // Update orbit weights
        std::array<float, 4> orbitWeights = editor->audioProcessor.getOrbitWeights();
        editor->screenComponent.setOrbitWeights(orbitWeights);
        
        // Update orbit shape and rate
        editor->screenComponent.setOrbitShape(editor->audioProcessor.getOrbitShape());
        editor->screenComponent.setOrbitRate(editor->audioProcessor.getOrbitRate());
        
        // Update orbit phase for dot animation
        float phase = editor->audioProcessor.getOrbitBlenderPhase();
        editor->screenComponent.setOrbitPhase(phase);
        
        // Force repaint of orbit visualization for smooth animation
        editor->screenComponent.repaint();
    }
    
    // Update playhead positions (yellow lines on waveform - one per active voice)
    std::vector<double> playheadPositions;
    std::vector<float> envelopeValues;
    editor->audioProcessor.getAllActivePlayheads(playheadPositions, envelopeValues);
    editor->screenComponent.setPlayheadPositions(playheadPositions, envelopeValues);
    
    // Check if waveform is empty and retry loading if sample data is now available
    // This handles the case where the default sample loads after the editor initializes
    static bool waveformLoadAttempted = false;
    if (editor->waveformInitialized && !waveformLoadAttempted) {
        std::vector<float> leftChannel, rightChannel;
        editor->audioProcessor.getSlotStereoSampleDataForVisualization(editor->currentSlotIndex, leftChannel, rightChannel);
        // If sample data is available, load it
        if (!leftChannel.empty()) {
            editor->updateWaveform(editor->currentSlotIndex);
            editor->updateWaveformVisualization();
            editor->updateAllSlotPreviews();
            // Force repaint to ensure waveform is visible
            editor->screenComponent.repaint();
            waveformLoadAttempted = true;  // Only try once
        }
    }
}

