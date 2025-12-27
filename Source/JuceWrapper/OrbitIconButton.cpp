#include "OrbitIconButton.h"
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>

OrbitIconButton::OrbitIconButton()
    : TextButton("")
{
}

OrbitIconButton::~OrbitIconButton()
{
}

void OrbitIconButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw button background with rounded corners (same as button 1)
    juce::Colour bgColour = isDown() ? juce::Colours::darkgrey.darker() : 
                            (shouldDrawButtonAsHighlighted ? juce::Colours::darkgrey.brighter() : juce::Colours::darkgrey);
    g.setColour(bgColour);
    
    // Draw rounded rectangle (same corner radius as default TextButton)
    float cornerRadius = 3.0f;
    g.fillRoundedRectangle(bounds, cornerRadius);
    
    // Draw figure-8 icon with dot
    float padding = 8.0f;
    juce::Rectangle<float> iconBounds = bounds.reduced(padding);
    drawFigure8Icon(g, iconBounds);
}

void OrbitIconButton::drawFigure8Icon(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    g.setColour(juce::Colours::white);
    
    float centerX = bounds.getCentreX();
    float centerY = bounds.getCentreY();
    float width = bounds.getWidth();
    float height = bounds.getHeight();
    
    // Scale to fit in bounds
    float size = std::min(width, height) * 0.7f;
    float a = size * 0.25f;  // Parameter for lemniscate (half the width)
    
    // Draw proper lemniscate (infinity symbol) using parametric equation
    // Lemniscate of Bernoulli: x = a*cos(t)/(1+sin²(t)), y = a*sin(t)*cos(t)/(1+sin²(t))
    juce::Path infinity;
    
    int numPoints = 100;  // More points for smoother curve
    bool firstPoint = true;
    
    for (int i = 0; i <= numPoints; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(numPoints) * juce::MathConstants<float>::twoPi;
        
        float denom = 1.0f + std::sin(t) * std::sin(t);
        float x = a * std::cos(t) / denom;
        float y = a * std::sin(t) * std::cos(t) / denom;
        
        float px = centerX + x;
        float py = centerY + y;
        
        if (firstPoint) {
            infinity.startNewSubPath(px, py);
            firstPoint = false;
        } else {
            infinity.lineTo(px, py);
        }
    }
    
    infinity.closeSubPath();
    
    // Draw the infinity symbol with thick, solid stroke
    juce::PathStrokeType stroke(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.strokePath(infinity, stroke);
    
    // Draw dot on the line (right side of the curve, on the upper right loop)
    // Position dot at t ≈ 0.3 radians (on the right loop)
    float dotT = 0.3f;
    float dotDenom = 1.0f + std::sin(dotT) * std::sin(dotT);
    float dotX = centerX + a * std::cos(dotT) / dotDenom;
    float dotY = centerY + a * std::sin(dotT) * std::cos(dotT) / dotDenom;
    float dotRadius = 3.5f;
    g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
}

