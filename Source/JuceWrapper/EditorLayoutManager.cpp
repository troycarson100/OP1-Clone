#include "EditorLayoutManager.h"
#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

EditorLayoutManager::EditorLayoutManager(Op1CloneAudioProcessorEditor* editor)
    : editor(editor)
{
}

void EditorLayoutManager::layoutComponents() {
    auto bounds = editor->getLocalBounds();
    
    // Top left: Master gain knob
    auto topLeftArea = bounds.removeFromLeft(150).removeFromTop(150).reduced(10);
    auto gainArea = topLeftArea;
    editor->gainSlider.setBounds(gainArea.removeFromTop(120).reduced(10));
    editor->volumeLabel.setBounds(gainArea.reduced(5));  // "Volume" text under the knob
    
    // Left side: Time Warp and Shift toggles (before screen)
    auto leftControlsArea = bounds.removeFromLeft(200).reduced(10);
    
    // Time-warp toggle
    editor->warpToggleButton.setBounds(leftControlsArea.removeFromTop(40).reduced(5));
    
    // Shift toggle button (wide enough for "shift" text)
    auto shiftButtonArea = leftControlsArea.removeFromTop(50);
    int shiftButtonWidth = 80;  // Wide enough for "shift" text
    editor->shiftToggleButton.setBounds(shiftButtonArea.removeFromLeft(shiftButtonWidth).reduced(5));
    
    // Middle: Screen component (30% wider than before)
    auto screenArea = bounds.removeFromLeft(bounds.getWidth() * 0.65 * 0.78); // 65% * 78% = ~51% (30% wider than 39%)
    auto screenBounds = screenArea.removeFromTop(static_cast<int>(screenArea.getHeight() * 0.6)); // 40% reduction = 60% of original
    editor->screenComponent.setBounds(screenBounds.reduced(10));
    
    // Sample name label (overlay on top of screen)
    auto screenComponentBounds = editor->screenComponent.getBounds();
    editor->sampleNameLabel.setBounds(screenComponentBounds.getX() + 10, 
                                      screenComponentBounds.getY() + 5, 
                                      screenComponentBounds.getWidth() - 20, 
                                      20);
    
    // Parameter displays at bottom of screen module (2 rows of 4)
    // Position them at the very bottom, ensuring they don't overlap the waveform
    // Add 5px padding around the group
    int paramDisplayHeight = 40;
    int paramDisplaySpacing = 5;
    int paramGroupPadding = 5; // Padding around the entire group
    int paramBottomPadding = 5; // Padding from bottom edge
    int totalParamHeight = paramDisplayHeight * 2 + paramDisplaySpacing;
    
    // Calculate available width for displays (with group padding)
    int availableWidth = screenComponentBounds.getWidth() - (paramGroupPadding * 2);
    int paramDisplayWidth = (availableWidth - paramDisplaySpacing * 3) / 4;
    
    // Calculate X start position (with left padding)
    int paramStartX = screenComponentBounds.getX() + paramGroupPadding;
    
    // Calculate Y positions from the bottom of the screen component
    int paramBottomRowY = screenComponentBounds.getBottom() - paramBottomPadding - paramDisplayHeight;
    int paramTopRowY = paramBottomRowY - paramDisplayHeight - paramDisplaySpacing;
    
    // Top row: Pitch, Start, End, Gain
    editor->paramDisplay1.setBounds(paramStartX, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay2.setBounds(paramStartX + paramDisplayWidth + paramDisplaySpacing, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay3.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 2, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay4.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 3, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    
    // Bottom row: Attack, Decay, Sustain, Release
    editor->paramDisplay5.setBounds(paramStartX, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay6.setBounds(paramStartX + paramDisplayWidth + paramDisplaySpacing, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay7.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 2, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay8.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 3, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    
    // ADSR visualization overlay - centered on waveform area within screen component
    // Get waveform bounds from screen component (relative to screen component)
    auto waveformBounds = editor->screenComponent.getWaveformBounds();
    // Convert to editor coordinates (waveformBounds is relative to screenComponent, need to add screenComponent position)
    juce::Rectangle<int> waveformBoundsInEditor(
        screenComponentBounds.getX() + waveformBounds.getX(),
        screenComponentBounds.getY() + waveformBounds.getY(),
        waveformBounds.getWidth(),
        waveformBounds.getHeight()
    );
    int adsrHeight = static_cast<int>(waveformBoundsInEditor.getHeight() * 0.4f);  // 40% of waveform height
    // Center it vertically on the waveform
    int adsrY = waveformBoundsInEditor.getCentreY() - adsrHeight / 2;
    editor->adsrVisualization.setBounds(waveformBoundsInEditor.getX(),
                                        adsrY,
                                        waveformBoundsInEditor.getWidth(),
                                        adsrHeight);
    
    // ADSR label (overlay in top right of screen)
    editor->adsrLabel.setBounds(screenComponentBounds.getX() + screenComponentBounds.getWidth() - 60,
                                screenComponentBounds.getY() + 5,
                                50,
                                20);
    
    // Parameter display label (overlay in top left of screen)
    editor->parameterDisplayLabel.setBounds(screenComponentBounds.getX() + 10,
                                            screenComponentBounds.getY() + 5,
                                            100,
                                            20);
    
    // BPM display label (overlay in top right of screen)
    editor->bpmDisplayLabel.setBounds(screenComponentBounds.getX() + screenComponentBounds.getWidth() - 100,
                                      screenComponentBounds.getY() + 5,
                                      90,
                                      20);
    
    // Under screen: Load sample button (directly below the screen)
    editor->loadSampleButton.setBounds(screenArea.removeFromTop(40).reduced(10));
    
    // Right side: Encoders in two horizontal rows
    auto encoderArea = bounds.reduced(10);
    int encoderSize = 80; // Much smaller encoders
    int encoderSpacing = 15;
    int rowSpacing = 20; // Vertical spacing between rows
    // Move left (closer to screen) - start closer to left edge instead of centering
    int startX = encoderArea.getX() + 20; // 20px from left edge instead of centered
    
    // Top row (encoders 1-4)
    int topRowY = encoderArea.getY() + encoderArea.getHeight() * 0.15; // Move up more (15% from top, was 25%)
    editor->encoder1.setBounds(startX, topRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder2.setBounds(startX + encoderSize + encoderSpacing, topRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder3.setBounds(startX + (encoderSize + encoderSpacing) * 2, topRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder4.setBounds(startX + (encoderSize + encoderSpacing) * 3, topRowY - encoderSize / 2, encoderSize, encoderSize);
    
    // Bottom row (encoders 5-8)
    int bottomRowY = topRowY + encoderSize + rowSpacing;
    editor->encoder5.setBounds(startX, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder6.setBounds(startX + encoderSize + encoderSpacing, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder7.setBounds(startX + (encoderSize + encoderSpacing) * 2, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder8.setBounds(startX + (encoderSize + encoderSpacing) * 3, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
    
    // MIDI status at bottom
    auto statusBounds = editor->getLocalBounds().removeFromBottom(25);
    editor->midiStatusComponent.setBounds(statusBounds);
}

