#include "EditorLayoutManager.h"
#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

EditorLayoutManager::EditorLayoutManager(Op1CloneAudioProcessorEditor* editor)
    : editor(editor)
{
}

void EditorLayoutManager::layoutComponents() {
    // OLD ADSR visualization removal code (COMMENTED OUT - replaced with pill component)
    /*
    // CRITICAL: Ensure ADSR visualization is removed if not in use
    // This must happen FIRST before any layout calculations
    // But only do this if not actively being dragged AND not fading
    if (!editor->isADSRDragging && editor->adsrFadeOutStartTime == 0) {
        if (editor->adsrVisualization.getParentComponent() != nullptr) {
            editor->adsrVisualization.getParentComponent()->removeChildComponent(&editor->adsrVisualization);
        }
        editor->adsrVisualization.setPaintingEnabled(false);  // CRITICAL: Disable painting
        editor->adsrVisualization.setVisible(false);
        editor->adsrVisualization.setAlpha(0.0f);
        editor->adsrVisualization.setBounds(0, 0, 0, 0);
    }
    */
    
    auto bounds = editor->getLocalBounds();
    
    // Top left: Master gain knob - keep in place
    auto topLeftArea = bounds.removeFromLeft(150).removeFromTop(150).reduced(10);
    auto gainArea = topLeftArea;
    editor->gainSlider.setBounds(gainArea.removeFromTop(120).reduced(10));
    editor->volumeLabel.setBounds(gainArea.reduced(5));  // "Volume" text under the knob
    
    // Left side: Time Warp and Shift toggles (before screen) - keep shift in place
    auto leftControlsArea = bounds.removeFromLeft(100).reduced(10);  // Reduced width
    
    // Time-warp toggle - COMMENTED OUT
    // editor->warpToggleButton.setBounds(leftControlsArea.removeFromTop(40).reduced(5));
    
    // Shift toggle button (wide enough for "shift" text) - keep in place
    auto shiftButtonArea = leftControlsArea.removeFromTop(50);
    int shiftButtonWidth = 80;  // Wide enough for "shift" text
    editor->shiftToggleButton.setBounds(shiftButtonArea.removeFromLeft(shiftButtonWidth).reduced(5));
    
    // Menu encoder (outside screen component, closer to shift button)
    int menuEncoderSize = 60;
    int menuEncoderX = bounds.getX() + 5;  // Move closer to left
    int menuEncoderY = bounds.getY() + 20;
    editor->menuEncoder.setBounds(menuEncoderX, menuEncoderY, menuEncoderSize, menuEncoderSize);
    
    // Adjust bounds to account for menu encoder (less padding)
    bounds.removeFromLeft(menuEncoderSize + 10);  // Reduced padding
    
    // Middle: Screen component - make it narrower to fit everything
    int screenWidth = 400;  // Fixed width instead of percentage
    auto screenArea = bounds.removeFromLeft(screenWidth);
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
    // Add more padding between sample slots and parameter displays
    int paramDisplayHeight = 40;
    int paramDisplaySpacing = 5;
    int paramGroupPadding = 5; // Padding around the entire group
    int paramBottomPadding = 5; // Padding from bottom edge
    int paddingAboveParams = 15; // More padding between sample slots and parameter displays
    int totalParamHeight = paramDisplayHeight * 2 + paramDisplaySpacing;
    
    // Calculate available width for displays (with group padding)
    int availableWidth = screenComponentBounds.getWidth() - (paramGroupPadding * 2);
    int paramDisplayWidth = (availableWidth - paramDisplaySpacing * 3) / 4;
    
    // Calculate X start position (with left padding)
    int paramStartX = screenComponentBounds.getX() + paramGroupPadding;
    
    // Calculate Y positions from the bottom of the screen component
    // Add extra padding above parameter displays (between sample slots and params)
    int paramBottomRowY = screenComponentBounds.getBottom() - paramBottomPadding - paramDisplayHeight;
    int paramTopRowY = paramBottomRowY - paramDisplayHeight - paramDisplaySpacing - paddingAboveParams + 15;  // Move top row down 15px for more padding
    
    // Top row: Pitch, Start, End, Gain
    editor->paramDisplay1.setBounds(paramStartX, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay2.setBounds(paramStartX + paramDisplayWidth + paramDisplaySpacing, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay3.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 2, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay4.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 3, paramTopRowY, paramDisplayWidth, paramDisplayHeight);
    
    // ADSR pill component - positioned above paramDisplay4, right-aligned with BPM display
    // Note: BPM display is positioned later, so we calculate its position manually
    int pillWidth = static_cast<int>(paramDisplayWidth * 0.6f);  // 40% less width (60% of original)
    int pillHeight = 20;  // Small pill height
    int pillSpacing = 3;  // Small gap above paramDisplay4
    // Calculate BPM display right edge (BPM is positioned at screenComponentBounds.getX() + screenComponentBounds.getWidth() - 100, width 90)
    int bpmRightEdge = screenComponentBounds.getX() + screenComponentBounds.getWidth() - 10;  // BPM right edge (100px from right, 90px width = 10px margin)
    // Right-align pill with BPM display
    int pillX = bpmRightEdge - pillWidth;
    int pillY = paramTopRowY - pillHeight - pillSpacing;
    editor->adsrPillComponent.setBounds(pillX, pillY, pillWidth, pillHeight);
    
    // Bottom row: Attack, Decay, Sustain, Release
    editor->paramDisplay5.setBounds(paramStartX, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay6.setBounds(paramStartX + paramDisplayWidth + paramDisplaySpacing, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay7.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 2, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    editor->paramDisplay8.setBounds(paramStartX + (paramDisplayWidth + paramDisplaySpacing) * 3, paramBottomRowY, paramDisplayWidth, paramDisplayHeight);
    
    // OLD ADSR visualization overlay (COMMENTED OUT - replaced with pill component)
    /*
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
    // Only set bounds for ADSR visualization if it's actually in the component tree AND painting is enabled
    // This prevents it from being visible on startup
    if (editor->adsrVisualization.getParentComponent() != nullptr && 
        editor->adsrVisualization.isPaintingEnabled() && 
        editor->isADSRDragging) {
        int adsrHeight = static_cast<int>(waveformBoundsInEditor.getHeight() * 0.4f);  // 40% of waveform height
        // Center it vertically on the waveform
        int adsrY = waveformBoundsInEditor.getCentreY() - adsrHeight / 2;
        editor->adsrVisualization.setBounds(waveformBoundsInEditor.getX(),
                                            adsrY,
                                            waveformBoundsInEditor.getWidth(),
                                            adsrHeight);
    }
    */
    
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
    // Load sample button is hidden (functionality moved to menu encoder center button)
    // editor->loadSampleButton.setBounds(screenArea.removeFromTop(40).reduced(10));
    
    // Right side: Encoders in two horizontal rows, top-aligned with screen
    // Move encoders left by positioning them closer to the screen and making them smaller
    int encoderSize = 70;  // Reduced from 80 to 70
    int encoderSpacing = 8;  // Reduced spacing
    int rowSpacing = 15; // Reduced vertical spacing
    
    // Get screen component top position for alignment
    int screenTop = screenComponentBounds.getY();
    
    // Calculate total width needed for 4 encoders
    int totalEncoderWidth = (encoderSize * 4) + (encoderSpacing * 3);
    
    // Position encoders immediately after screen (minimal gap)
    int encoderStartX = screenComponentBounds.getRight() + 5; // Minimal gap after screen
    
    // Check if encoders would overflow - if so, reduce size further
    int maxAvailableWidth = editor->getWidth() - encoderStartX - 20; // Leave 20px margin on right
    if (totalEncoderWidth > maxAvailableWidth) {
        // Recalculate to fit
        encoderSize = (maxAvailableWidth - (encoderSpacing * 3)) / 4;
        totalEncoderWidth = (encoderSize * 4) + (encoderSpacing * 3);
    }
    
    // Position encoders so top row aligns with top of screen
    int topRowY = screenTop + encoderSize / 2; // Center encoders vertically on top edge
    int bottomRowY = topRowY + encoderSize + rowSpacing;
    
    // Top row (encoders 1-4)
    editor->encoder1.setBounds(encoderStartX, topRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder2.setBounds(encoderStartX + encoderSize + encoderSpacing, topRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder3.setBounds(encoderStartX + (encoderSize + encoderSpacing) * 2, topRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder4.setBounds(encoderStartX + (encoderSize + encoderSpacing) * 3, topRowY - encoderSize / 2, encoderSize, encoderSize);
    
    // Bottom row (encoders 5-8)
    editor->encoder5.setBounds(encoderStartX, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder6.setBounds(encoderStartX + encoderSize + encoderSpacing, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder7.setBounds(encoderStartX + (encoderSize + encoderSpacing) * 2, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
    editor->encoder8.setBounds(encoderStartX + (encoderSize + encoderSpacing) * 3, bottomRowY - encoderSize / 2, encoderSize, encoderSize);
    
    // 5 square buttons below encoders, spreading the same width as encoders
    int buttonSpacing = 8; // Reduced spacing
    int buttonSize = (totalEncoderWidth - (buttonSpacing * 4)) / 5; // Calculate button size to match encoder width
    int buttonStartX = encoderStartX; // Align with encoder start
    int buttonY = bottomRowY + encoderSize / 2 + rowSpacing; // Position below bottom row of encoders
    
    editor->squareButton1.setBounds(buttonStartX, buttonY, buttonSize, buttonSize);
    editor->squareButton2.setBounds(buttonStartX + buttonSize + buttonSpacing, buttonY, buttonSize, buttonSize);
    editor->squareButton3.setBounds(buttonStartX + (buttonSize + buttonSpacing) * 2, buttonY, buttonSize, buttonSize);
    editor->squareButton4.setBounds(buttonStartX + (buttonSize + buttonSpacing) * 3, buttonY, buttonSize, buttonSize);
    editor->squareButton5.setBounds(buttonStartX + (buttonSize + buttonSpacing) * 4, buttonY, buttonSize, buttonSize);
    
    // MIDI status at bottom
    auto statusBounds = editor->getLocalBounds().removeFromBottom(25);
    editor->midiStatusComponent.setBounds(statusBounds);
}

