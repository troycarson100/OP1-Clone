#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Hardware-style endless encoder with push button
// Resembles OP-1 style encoders
class EncoderComponent : public juce::Component,
                         public juce::Button::Listener,
                         public juce::Slider::Listener {
public:
    EncoderComponent(const juce::String& name);
    ~EncoderComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Override mouse events to handle circular button clicks
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
    // Button callback
    void buttonClicked(juce::Button* button) override;
    
    // Slider callback for encoder rotation
    void sliderValueChanged(juce::Slider* slider) override;
    
    // Get current value (0.0 to 1.0, wraps around)
    float getValue() const { return encoderSlider.getValue() / 100.0f; }
    
    // Set value (0.0 to 1.0)
    void setValue(float value) { encoderSlider.setValue(value * 100.0f, juce::dontSendNotification); }
    
    // Callback for value changes
    std::function<void(float)> onValueChanged;
    
    // Callback for button press
    std::function<void()> onButtonPressed;
    
private:
    juce::String encoderName;
    
    // Endless encoder slider (0-100, wraps around)
    juce::Slider encoderSlider;
    
    // Push button in center
    juce::TextButton centerButton;
    
    // Visual state
    float rotationAngle; // Current visual rotation (can exceed 2π for endless rotation)
    float lastSliderValue; // Track last slider value for delta calculation
    bool buttonPressed; // Track button press state for visual feedback
    
    // Hardware-style colors
    juce::Colour encoderColor;
    juce::Colour buttonColor;
    juce::Colour indicatorColor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EncoderComponent)
};

