#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

// Custom button with piano icon SVG
class PianoIconButton : public juce::TextButton {
public:
    PianoIconButton();
    ~PianoIconButton() override;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    std::unique_ptr<juce::Drawable> pianoIcon;
    void loadPianoIcon();
};

