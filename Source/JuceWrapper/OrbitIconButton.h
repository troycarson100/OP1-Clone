#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

// Custom button with figure-8 (infinity) icon with dot
class OrbitIconButton : public juce::TextButton {
public:
    OrbitIconButton();
    ~OrbitIconButton() override;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    void drawFigure8Icon(juce::Graphics& g, const juce::Rectangle<float>& bounds);
};

