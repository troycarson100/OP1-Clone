#include "KeyboardButton.h"

KeyboardButton::KeyboardButton()
    : Button("")
{
}

KeyboardButton::~KeyboardButton()
{
}

void KeyboardButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw button background
    juce::Colour bgColour = isDown() ? juce::Colours::darkgrey.darker() : 
                            (shouldDrawButtonAsHighlighted ? juce::Colours::darkgrey.brighter() : juce::Colours::darkgrey);
    g.setColour(bgColour);
    g.fillRect(bounds);
    
    // Draw border
    g.setColour(juce::Colours::black);
    g.drawRect(bounds, 1.0f);
    
    // Draw keyboard icon
    drawKeyboardIcon(g, bounds.reduced(4.0f));
}

void KeyboardButton::drawKeyboardIcon(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    g.setColour(juce::Colours::white);
    
    // Draw a simple keyboard icon (piano keys style)
    float keyWidth = bounds.getWidth() / 7.0f;  // 7 keys
    float keyHeight = bounds.getHeight();
    
    // Draw white keys
    for (int i = 0; i < 7; ++i) {
        float x = bounds.getX() + i * keyWidth;
        juce::Rectangle<float> keyRect(x, bounds.getY(), keyWidth - 1.0f, keyHeight);
        g.fillRect(keyRect);
        g.setColour(juce::Colours::black);
        g.drawRect(keyRect, 0.5f);
        g.setColour(juce::Colours::white);
    }
    
    // Draw black keys (on top of white keys)
    g.setColour(juce::Colours::black);
    float blackKeyWidth = keyWidth * 0.6f;
    float blackKeyHeight = keyHeight * 0.6f;
    
    // Black keys at positions 1, 2, 4, 5, 6 (between white keys)
    int blackKeyPositions[] = {1, 2, 4, 5, 6};
    for (int pos : blackKeyPositions) {
        float x = bounds.getX() + pos * keyWidth - blackKeyWidth / 2.0f;
        juce::Rectangle<float> blackKeyRect(x, bounds.getY(), blackKeyWidth, blackKeyHeight);
        g.fillRect(blackKeyRect);
    }
}



