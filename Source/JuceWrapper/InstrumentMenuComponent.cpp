#include "InstrumentMenuComponent.h"

InstrumentMenuComponent::InstrumentMenuComponent()
    : selectedIndex(0)
{
    instruments.push_back("Sampler");
    instruments.push_back("JNO");
    setOpaque(true);  // Opaque to fully cover background
    setVisible(false);  // Start hidden
}

InstrumentMenuComponent::~InstrumentMenuComponent()
{
}

void InstrumentMenuComponent::setMenuVisible(bool visible)
{
    Component::setVisible(visible);
    repaint();
}

void InstrumentMenuComponent::setSelectedIndex(int index)
{
    if (index >= 0 && index < static_cast<int>(instruments.size())) {
        selectedIndex = index;
        repaint();
    }
}

juce::String InstrumentMenuComponent::getSelectedInstrument() const
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(instruments.size())) {
        return instruments[selectedIndex];
    }
    return "";
}

void InstrumentMenuComponent::paint(juce::Graphics& g)
{
    if (!isVisible()) return;

    auto bounds = getLocalBounds();
    
    // Draw black background covering entire screen component
    g.setColour(juce::Colours::black);
    g.fillRect(bounds);

    // Draw instrument list on left side, full width items
    g.setFont(18.0f);
    int itemHeight = 50;
    int padding = 10;
    int startY = padding;

    for (size_t i = 0; i < instruments.size(); ++i) {
        int itemY = startY + static_cast<int>(i * itemHeight);
        // Items span almost full width, aligned to left
        juce::Rectangle<int> itemBounds(padding, itemY, bounds.getWidth() - padding * 2, itemHeight - padding);

        // Highlight selected item - white background with black text
        if (static_cast<int>(i) == selectedIndex) {
            g.setColour(juce::Colours::white);  // White background for selected
            g.fillRect(itemBounds);
            g.setColour(juce::Colours::black);  // Black text for selected
            g.setFont(juce::Font(18.0f, juce::Font::bold));  // Bold text for selected
        } else {
            // Unselected items - white text on black background
            g.setColour(juce::Colours::white);  // White text for unselected
            g.setFont(18.0f);  // Regular font for unselected
        }
        
        // Left-align text with some padding
        g.drawText(instruments[i], itemBounds.reduced(10, 0), juce::Justification::centredLeft);
    }
}

void InstrumentMenuComponent::resized()
{
    // Menu fills entire component bounds
}

