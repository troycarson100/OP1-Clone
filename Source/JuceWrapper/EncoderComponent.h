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
    
    // Override mouse events for visual feedback
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
    
    // Callback for drag start/end
    std::function<void()> onDragStart;
    std::function<void()> onDragEnd;
    
private:
    // Custom slider to track drag start/end
    class DragTrackingSlider : public juce::Slider {
    public:
        DragTrackingSlider(EncoderComponent* parent) : parentComponent(parent) {}
        
        void startedDragging() override {
            if (parentComponent && parentComponent->onDragStart) {
                parentComponent->onDragStart();
            }
        }
        
        void stoppedDragging() override {
            if (parentComponent && parentComponent->onDragEnd) {
                parentComponent->onDragEnd();
            }
        }
        
    private:
        EncoderComponent* parentComponent;
    };
    
    // Small transparent component that covers only the center circle to intercept clicks
    class CenterButtonOverlay : public juce::Component {
    public:
        CenterButtonOverlay(EncoderComponent* parent) : parentComponent(parent) {
            // Intercept clicks to check hitTest, but not mouse moves/drags
            // hitTest will filter to only the center circle
            setInterceptsMouseClicks(true, false);
            setOpaque(false);
        }
        
        bool hitTest(int x, int y) override {
            // Only respond to clicks within the circular area (the lighter gray center circle)
            // The overlay is a square, but we only want the circular part to intercept clicks
            auto bounds = getLocalBounds();
            int size = bounds.getWidth(); // Overlay is square (buttonSize = buttonRadius * 2)
            int centerX = size / 2;
            int centerY = size / 2;
            // The button radius is half the overlay size (since overlay = buttonRadius * 2)
            int buttonRadius = size / 2;
            
            int dx = x - centerX;
            int dy = y - centerY;
            int distanceSquared = dx * dx + dy * dy;
            int radiusSquared = buttonRadius * buttonRadius;
            
            // Only intercept clicks within the center circle (lighter gray area)
            // Clicks outside this circle (on the outer ring/darker gray) will pass through
            bool isInCircle = distanceSquared <= radiusSquared;
            return isInCircle;
        }
        
        void mouseDown(const juce::MouseEvent&) override {
            // This will only be called if hitTest returned true (click is in circle)
            if (parentComponent && parentComponent->onButtonPressed) {
                parentComponent->onButtonPressed();
            }
        }
        
        void paint(juce::Graphics&) override {
            // Completely transparent - no visual rendering
        }
        
    private:
        EncoderComponent* parentComponent;
    };
    
    juce::String encoderName;
    
    // Endless encoder slider (0-100, wraps around)
    DragTrackingSlider encoderSlider;
    
    // Push button in center - invisible button that draws nothing
    class InvisibleButton : public juce::TextButton {
    public:
        InvisibleButton() : juce::TextButton() {
            setButtonText("");
            setClickingTogglesState(false);
            // Set all colors to fully transparent
            setColour(juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
            setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
            setColour(juce::TextButton::textColourOffId, juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
            setColour(juce::TextButton::textColourOnId, juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
            setComponentEffect(nullptr);
        }
        
        void paint(juce::Graphics&) override {
            // Draw nothing - completely invisible
        }
    };
    
    InvisibleButton centerButton;
    
    // Visual state
    float rotationAngle; // Current visual rotation (can exceed 2π for endless rotation)
    float lastSliderValue; // Track last slider value for delta calculation
    bool buttonPressed; // Track button press state for visual feedback
    
    // Hardware-style colors
    juce::Colour encoderColor;
    juce::Colour buttonColor;
    juce::Colour indicatorColor;
    
    // Make onButtonPressed accessible to overlay
    friend class CenterButtonOverlay;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EncoderComponent)
};

