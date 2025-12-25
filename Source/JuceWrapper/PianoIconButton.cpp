#include "PianoIconButton.h"
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

PianoIconButton::PianoIconButton()
    : TextButton("")
{
    loadPianoIcon();
}

PianoIconButton::~PianoIconButton()
{
}

void PianoIconButton::loadPianoIcon()
{
    // Try multiple paths to find the SVG file
    juce::File svgFile;
    
    // First, try from project root (most reliable for development)
    svgFile = juce::File::getCurrentWorkingDirectory()
        .getChildFile("assets").getChildFile("Piano_Icon.svg");
    
    // If not found, try relative to executable (for standalone app)
    // macOS app bundle: App.app/Contents/MacOS/App
    if (!svgFile.existsAsFile()) {
        auto exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        // Go up to app bundle root
        auto appBundle = exeFile.getParentDirectory().getParentDirectory().getParentDirectory();
        
        // Try in Resources folder first (standard macOS location)
        svgFile = appBundle.getChildFile("Contents").getChildFile("Resources")
            .getChildFile("assets").getChildFile("Piano_Icon.svg");
        
        // Also try at app bundle root
        if (!svgFile.existsAsFile()) {
            svgFile = appBundle.getChildFile("assets").getChildFile("Piano_Icon.svg");
        }
    }
    
    // Try absolute path from common locations
    if (!svgFile.existsAsFile()) {
        // Try from build directory
        svgFile = juce::File("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/assets/Piano_Icon.svg");
    }
    
    if (svgFile.existsAsFile()) {
        auto svgData = svgFile.loadFileAsString();
        if (svgData.isNotEmpty()) {
            auto xml = juce::XmlDocument::parse(svgData);
            if (xml != nullptr) {
                pianoIcon = juce::Drawable::createFromSVG(*xml);
            }
        }
    }
}

void PianoIconButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw button background with rounded corners (same as button 2)
    juce::Colour bgColour = isDown() ? juce::Colours::darkgrey.darker() : 
                            (shouldDrawButtonAsHighlighted ? juce::Colours::darkgrey.brighter() : juce::Colours::darkgrey);
    g.setColour(bgColour);
    
    // Draw rounded rectangle (same corner radius as default TextButton)
    float cornerRadius = 3.0f;
    g.fillRoundedRectangle(bounds, cornerRadius);
    
    // No border - match button 2 style
    
    // Draw piano icon if loaded
    if (pianoIcon != nullptr) {
        // Scale icon to fit button with padding, then make it 40% smaller
        float padding = 8.0f;
        juce::Rectangle<float> iconBounds = bounds.reduced(padding);
        
        // Center the icon
        float iconAspectRatio = 187.0f / 183.0f;  // From SVG viewBox
        float buttonAspectRatio = bounds.getWidth() / bounds.getHeight();
        
        if (iconAspectRatio > buttonAspectRatio) {
            // Icon is wider - fit to width
            float iconHeight = iconBounds.getWidth() / iconAspectRatio;
            iconBounds = iconBounds.withSizeKeepingCentre(iconBounds.getWidth(), iconHeight);
        } else {
            // Icon is taller - fit to height
            float iconWidth = iconBounds.getHeight() * iconAspectRatio;
            iconBounds = iconBounds.withSizeKeepingCentre(iconWidth, iconBounds.getHeight());
        }
        
        // Make icon 40% smaller (scale by 0.6)
        iconBounds = iconBounds.withSizeKeepingCentre(iconBounds.getWidth() * 0.6f, iconBounds.getHeight() * 0.6f);
        
        g.setColour(juce::Colours::white);
        pianoIcon->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, 1.0f);
    }
}

