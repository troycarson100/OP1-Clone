#include "EncoderComponent.h"
#include <cmath>

EncoderComponent::EncoderComponent(const juce::String& name)
    : encoderName(name)
    , rotationAngle(0.0f)
    , lastSliderValue(50.0f)
    , buttonPressed(false)
    , encoderColor(juce::Colour(0xFF444444))
    , buttonColor(juce::Colour(0xFF666666))
    , indicatorColor(juce::Colours::white)
{
    // Setup endless encoder slider (0-100, no text box, starts at middle)
    encoderSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    encoderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    encoderSlider.setRange(0.0, 100.0, 0.1);
    encoderSlider.setValue(50.0); // Start at middle for endless feel
    encoderSlider.addListener(this);
    encoderSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::transparentBlack);
    encoderSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::transparentBlack);
    encoderSlider.setColour(juce::Slider::thumbColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(&encoderSlider);
    
    // Setup center button (circular, no text, fully transparent - we draw it ourselves)
    centerButton.setButtonText("");
    centerButton.addListener(this);
    centerButton.setClickingTogglesState(false);
    centerButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    centerButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    centerButton.setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    centerButton.setColour(juce::TextButton::textColourOnId, juce::Colours::transparentBlack);
    // Button is invisible - we draw it ourselves and handle clicks in mouseDown
    centerButton.setComponentEffect(nullptr);
    centerButton.setVisible(false); // Hide the button, we handle clicks ourselves
    addChildComponent(&centerButton); // Add as child but don't make visible
    
    setOpaque(false);
}

EncoderComponent::~EncoderComponent() {
    encoderSlider.removeListener(this);
    centerButton.removeListener(this);
}

void EncoderComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    int size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    int centerX = bounds.getCentreX();
    int centerY = bounds.getCentreY();
    int radius = size / 2 - 5;
    
    // Draw encoder ring (hardware style)
    g.setColour(encoderColor);
    g.fillEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2);
    
    // Draw outer ring highlight
    g.setColour(encoderColor.brighter(0.2f));
    g.drawEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2, 2.0f);
    
    // Draw inner ring (button area) - perfect circle
    int buttonRadius = static_cast<int>(radius * 0.35f);
    g.setColour(buttonColor);
    g.fillEllipse(static_cast<float>(centerX - buttonRadius), static_cast<float>(centerY - buttonRadius), 
                  static_cast<float>(buttonRadius * 2), static_cast<float>(buttonRadius * 2));
    
    // Draw button highlight
    g.setColour(buttonColor.brighter(0.15f));
    g.drawEllipse(static_cast<float>(centerX - buttonRadius), static_cast<float>(centerY - buttonRadius), 
                  static_cast<float>(buttonRadius * 2), static_cast<float>(buttonRadius * 2), 1.5f);
    
    // Draw button pressed state
    if (buttonPressed) {
        g.setColour(buttonColor.darker(0.3f));
        g.fillEllipse(static_cast<float>(centerX - buttonRadius * 0.8f), static_cast<float>(centerY - buttonRadius * 0.8f), 
                      static_cast<float>(buttonRadius * 1.6f), static_cast<float>(buttonRadius * 1.6f));
    }
    
    // Draw indicator line (shows rotation position)
    // Normalize angle for display (smooth, no jumps, single line only)
    float twoPi = 2.0f * juce::MathConstants<float>::pi;
    
    // Normalize to 0-2π range using fmod for smooth, continuous rotation
    float displayAngle = std::fmod(rotationAngle, twoPi);
    if (displayAngle < 0.0f) {
        displayAngle += twoPi;
    }
    
    float indicatorLength = radius * 0.25f;
    float startRadius = radius - indicatorLength;
    float endRadius = radius - 2.0f; // Slight inset from edge
    
    // Draw single indicator line
    g.setColour(indicatorColor);
    float cosAngle = std::cos(displayAngle);
    float sinAngle = std::sin(displayAngle);
    g.drawLine(centerX + cosAngle * startRadius,
               centerY - sinAngle * startRadius,
               centerX + cosAngle * endRadius,
               centerY - sinAngle * endRadius,
               2.5f);
}

void EncoderComponent::resized() {
    auto bounds = getLocalBounds();
    int size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    
    // Encoder slider takes full area (but we draw custom)
    encoderSlider.setBounds(bounds);
    
    // Center button (perfect circle, matches the drawn circle)
    int buttonRadius = static_cast<int>(size * 0.35f);
    int buttonSize = buttonRadius * 2;
    int centerX = bounds.getCentreX();
    int centerY = bounds.getCentreY();
    centerButton.setBounds(centerX - buttonRadius, centerY - buttonRadius, buttonSize, buttonSize);
}

void EncoderComponent::mouseDown(const juce::MouseEvent& e) {
    // Check if click is within the circular button area
    auto bounds = getLocalBounds();
    int size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    int centerX = bounds.getCentreX();
    int centerY = bounds.getCentreY();
    int buttonRadius = static_cast<int>(size * 0.35f);
    
    int dx = e.x - centerX;
    int dy = e.y - centerY;
    int distanceSquared = dx * dx + dy * dy;
    int radiusSquared = buttonRadius * buttonRadius;
    
    // If click is within the circular button area, trigger button press
    if (distanceSquared <= radiusSquared) {
        buttonPressed = true;
        repaint(); // Update visual feedback
        
        if (onButtonPressed) {
            onButtonPressed();
        }
    }
}

void EncoderComponent::mouseUp(const juce::MouseEvent& e) {
    buttonPressed = false;
    repaint();
}

void EncoderComponent::buttonClicked(juce::Button* button) {
    if (button == &centerButton) {
        if (onButtonPressed) {
            onButtonPressed();
        }
    }
}

void EncoderComponent::sliderValueChanged(juce::Slider* slider) {
    if (slider == &encoderSlider) {
        // Endless encoder: track delta changes smoothly, handle wrapping
        float currentValue = encoderSlider.getValue();
        float delta = currentValue - lastSliderValue;
        
        // Handle wrapping at boundaries - detect large jumps that indicate wrap
        const float wrapThreshold = 50.0f; // If delta is larger than this, it's a wrap
        
        if (std::abs(delta) > wrapThreshold) {
            // This is a wrap - calculate the correct delta
            if (delta > 0) {
                // Wrapped from high to low (e.g., 99 -> 1)
                delta = delta - 100.0f;
            } else {
                // Wrapped from low to high (e.g., 1 -> 99)
                delta = delta + 100.0f;
            }
        }
        
        // Convert delta to rotation angle (smooth, continuous)
        float twoPi = 2.0f * juce::MathConstants<float>::pi;
        float angleDelta = delta * 0.01f * twoPi; // Map 0-100 range to 0-2π
        
        // Accumulate rotation angle smoothly
        rotationAngle += angleDelta;
        
        // Handle boundary resets (when slider hits 0 or 100) - reset slider position but keep rotation
        if (currentValue >= 99.9f) {
            // Reset to middle, but rotation continues smoothly
            encoderSlider.setValue(50.0f, juce::dontSendNotification);
            lastSliderValue = 50.0f;
        } else if (currentValue <= 0.1f) {
            // Reset to middle, but rotation continues smoothly
            encoderSlider.setValue(50.0f, juce::dontSendNotification);
            lastSliderValue = 50.0f;
        } else {
            // Normal rotation - update last value
            lastSliderValue = currentValue;
        }
        
        // Normalize rotation angle periodically to prevent overflow (but keep smooth display)
        // Only normalize if it gets very large, and preserve fractional part
        if (std::abs(rotationAngle) > twoPi * 100.0f) {
            float fullRotations = std::floor(rotationAngle / twoPi);
            rotationAngle = rotationAngle - (fullRotations * twoPi);
        }
        
        // Calculate normalized value (0.0 to 1.0) based on accumulated rotation
        // Use the slider's current position (0-100) mapped to 0-1, not rotation angle
        // This ensures the callback gets the actual slider position
        float normalizedValue = currentValue / 100.0f;
        
        // Trigger callback with normalized value
        if (onValueChanged) {
            onValueChanged(normalizedValue);
        }
        
        repaint();
    }
}

