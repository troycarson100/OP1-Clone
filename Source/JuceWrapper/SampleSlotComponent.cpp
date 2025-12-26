#include "SampleSlotComponent.h"
#include <array>

SampleSlotComponent::SampleSlotComponent()
    : selectedSlotIndex(0)  // Start with slot A selected
{
    setOpaque(false);
}

SampleSlotComponent::~SampleSlotComponent()
{
}

void SampleSlotComponent::setSlotAPreview(const std::vector<float>& sampleData)
{
    // Legacy method - delegate to new method
    setSlotPreview(0, sampleData);
}

void SampleSlotComponent::setSlotPreview(int slotIndex, const std::vector<float>& sampleData)
{
    if (slotIndex >= 0 && slotIndex < NUM_SLOTS) {
        slotPreviewData[slotIndex] = sampleData;
        repaint();
    }
}

void SampleSlotComponent::setSelectedSlot(int slotIndex)
{
    if (slotIndex >= 0 && slotIndex < NUM_SLOTS) {
        selectedSlotIndex = slotIndex;
        repaint();
    }
}

void SampleSlotComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Draw 5 slots horizontally
    int slotWidth = bounds.getWidth() / NUM_SLOTS;
    int slotHeight = bounds.getHeight();
    
    for (int i = 0; i < NUM_SLOTS; ++i) {
        int slotX = i * slotWidth;
        juce::Rectangle<int> slotBounds(slotX, 0, slotWidth, slotHeight);
        
        // Draw border between slots (except first)
        if (i > 0) {
            g.setColour(juce::Colour(0xFF666666));  // Light gray divider
            g.drawVerticalLine(slotX, 0.0f, static_cast<float>(slotHeight));
        }
        
        drawSlot(g, i, slotBounds);
    }
}

void SampleSlotComponent::drawSlot(juce::Graphics& g, int slotIndex, const juce::Rectangle<int>& bounds)
{
    // Background - black
    g.setColour(juce::Colours::black);
    g.fillRect(bounds);
    
    // Draw border - thicker white for selected slot, lighter gray for others
    if (slotIndex == selectedSlotIndex) {
        g.setColour(juce::Colours::white);  // White border for selected
        g.drawRect(bounds, 3);  // Thicker border (3px)
    } else {
        g.setColour(juce::Colour(0xFF333333));  // Lighter gray border for unselected
        g.drawRect(bounds, 1);
    }
    
    // Slot label (A, B, C, D, E)
    char slotLabel = 'A' + slotIndex;
    juce::String labelText = juce::String::charToString(slotLabel);
    
    // Draw label at bottom-left
    g.setColour(juce::Colour(0xFFCCCCCC));  // Light gray text
    g.setFont(12.0f);
    juce::Rectangle<int> labelBounds = bounds.reduced(4);
    g.drawText(labelText, labelBounds, juce::Justification::bottomLeft);
    
    // Draw waveform preview if available for this slot
    if (!slotPreviewData[slotIndex].empty()) {
        juce::Rectangle<int> previewBounds = bounds.reduced(4, 20);  // Leave space for label
        drawWaveformPreview(g, previewBounds, slotPreviewData[slotIndex]);
        
        // Draw blue selection bar above slot if it has a sample
        g.setColour(juce::Colour(0xFF4A90E2));  // Bright blue
        int barHeight = 3;
        g.fillRect(bounds.getX(), bounds.getY() - barHeight, bounds.getWidth(), barHeight);
    }
}

void SampleSlotComponent::drawWaveformPreview(juce::Graphics& g, const juce::Rectangle<int>& bounds, const std::vector<float>& data)
{
    if (data.empty() || bounds.getWidth() <= 0 || bounds.getHeight() <= 0) {
        return;
    }
    
    // Draw waveform preview in blue
    g.setColour(juce::Colour(0xFF5BA3E8));  // Slightly lighter blue for waveform
    
    size_t numPoints = juce::jmin(data.size(), static_cast<size_t>(bounds.getWidth()));
    if (numPoints < 2) return;
    
    juce::Path waveformPath;
    float centerY = bounds.getCentreY();
    float halfHeight = bounds.getHeight() * 0.4f;  // Use 40% of height for waveform
    
    // Start path
    float firstX = static_cast<float>(bounds.getX());
    float firstY = centerY - (data[0] * halfHeight);
    waveformPath.startNewSubPath(firstX, firstY);
    
    // Draw waveform
    for (size_t i = 1; i < numPoints; ++i) {
        float x = static_cast<float>(bounds.getX()) + (static_cast<float>(i) / static_cast<float>(numPoints - 1)) * static_cast<float>(bounds.getWidth());
        float y = centerY - (data[i] * halfHeight);
        waveformPath.lineTo(x, y);
    }
    
    // Create filled shape (mirror for top and bottom)
    juce::Path filledPath = waveformPath;
    filledPath.lineTo(static_cast<float>(bounds.getRight()), centerY);
    filledPath.lineTo(static_cast<float>(bounds.getX()), centerY);
    filledPath.closeSubPath();
    
    g.fillPath(filledPath);
}

void SampleSlotComponent::resized()
{
    // Component fills its bounds
}

