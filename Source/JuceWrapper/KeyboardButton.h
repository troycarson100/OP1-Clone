#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Custom button with keyboard icon for instrument select
class KeyboardButton : public juce::Button {
public:
    KeyboardButton();
    ~KeyboardButton() override;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    void drawKeyboardIcon(juce::Graphics& g, const juce::Rectangle<float>& bounds);
};


