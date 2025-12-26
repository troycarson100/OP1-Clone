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
        // Force a repaint to update the preview
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
    if (!slotPreviewData[slotIndex].empty() && slotPreviewData[slotIndex].size() > 0) {
        // Leave space for label at bottom (about 16px for text), and padding on sides
        // Make waveform smaller - use more padding
        int topPadding = 6;
        int bottomPadding = 18;  // Space for label
        int sidePadding = 6;
        juce::Rectangle<int> previewBounds(
            bounds.getX() + sidePadding,
            bounds.getY() + topPadding,
            bounds.getWidth() - (2 * sidePadding),
            bounds.getHeight() - topPadding - bottomPadding
        );
        
        // Always draw preview if we have data and valid bounds
        if (previewBounds.getHeight() > 2 && previewBounds.getWidth() > 2) {
            drawWaveformPreview(g, previewBounds, slotPreviewData[slotIndex]);
        }
        
        // Draw blue indicator bar at top of slot if it has a sample
        g.setColour(juce::Colour(0xFF4A90E2));  // Bright blue
        int barHeight = 2;
        g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), barHeight);
    }
}

void SampleSlotComponent::drawWaveformPreview(juce::Graphics& g, const juce::Rectangle<int>& bounds, const std::vector<float>& data)
{
    if (data.empty() || bounds.getWidth() <= 0 || bounds.getHeight() <= 0) {
        return;
    }
    
    int displayWidth = bounds.getWidth();
    int displayHeight = bounds.getHeight();
    
    if (displayWidth < 2 || displayHeight < 2) return;
    
    // Draw waveform preview in white
    g.setColour(juce::Colours::white);
    
    // Find peak value for normalization
    float maxAbs = 0.0f;
    for (float sample : data) {
        float absVal = std::abs(sample);
        if (absVal > maxAbs) {
            maxAbs = absVal;
        }
    }
    
    // If all values are zero or very small, don't draw
    if (maxAbs < 0.001f) return;
    
    // Normalize factor (scale to use less of the height to make it smaller/thinner, centered)
    float normalizeFactor = (displayHeight * 0.3f) / maxAbs;  // Use 30% of height to make it less tall
    
    float centerY = bounds.getCentreY();
    float leftX = static_cast<float>(bounds.getX());
    float rightX = static_cast<float>(bounds.getRight());
    
    // For each pixel column, find min/max values to create a proper waveform visualization
    // This creates a shrunken/mini version that looks like the actual waveform
    float samplesPerPixel = static_cast<float>(data.size()) / static_cast<float>(displayWidth);
    
    // Build a path for the filled waveform
    juce::Path waveformPath;
    std::vector<float> maxYValues;
    std::vector<float> minYValues;
    
    // First pass: collect min/max for each pixel column
    for (int x = 0; x < displayWidth; ++x) {
        // Calculate which samples correspond to this pixel column
        float startSample = x * samplesPerPixel;
        float endSample = (x + 1) * samplesPerPixel;
        
        // Find min and max in this range
        bool foundSample = false;
        float minVal = 0.0f;
        float maxVal = 0.0f;
        int startIdx = static_cast<int>(startSample);
        int endIdx = static_cast<int>(std::ceil(endSample));
        
        // Ensure we don't go out of bounds
        if (startIdx < 0) startIdx = 0;
        if (endIdx > static_cast<int>(data.size())) endIdx = static_cast<int>(data.size());
        if (endIdx <= startIdx) {
            if (startIdx < static_cast<int>(data.size())) {
                endIdx = startIdx + 1;
            } else {
                // No samples for this pixel - use center line
                maxYValues.push_back(centerY);
                minYValues.push_back(centerY);
                continue;
            }
        }
        
        // Find min/max in this pixel's sample range
        for (int i = startIdx; i < endIdx && i < static_cast<int>(data.size()); ++i) {
            float val = data[i];
            if (!foundSample) {
                minVal = val;
                maxVal = val;
                foundSample = true;
            } else {
                if (val < minVal) minVal = val;
                if (val > maxVal) maxVal = val;
            }
        }
        
        // If we found samples, calculate Y positions; otherwise use center
        if (foundSample) {
            float maxY = centerY - (maxVal * normalizeFactor);
            float minY = centerY - (minVal * normalizeFactor);
            maxYValues.push_back(maxY);
            minYValues.push_back(minY);
        } else {
            maxYValues.push_back(centerY);
            minYValues.push_back(centerY);
        }
    }
    
    // Build filled path: top curve (max values) going left to right, then bottom curve (min values) going right to left
    if (!maxYValues.empty() && maxYValues.size() == minYValues.size()) {
        // Start at first max point (top left)
        waveformPath.startNewSubPath(leftX, maxYValues[0]);
        
        // Draw top curve (max values) from left to right
        for (size_t i = 1; i < maxYValues.size(); ++i) {
            float x = leftX + (static_cast<float>(i) / static_cast<float>(maxYValues.size() - 1)) * (rightX - leftX);
            waveformPath.lineTo(x, maxYValues[i]);
        }
        
        // Draw bottom curve (min values) from right to left
        for (int i = static_cast<int>(minYValues.size()) - 1; i >= 0; --i) {
            float x = leftX + (static_cast<float>(i) / static_cast<float>(minYValues.size() - 1)) * (rightX - leftX);
            waveformPath.lineTo(x, minYValues[i]);
        }
        
        // Close the path back to start
        waveformPath.closeSubPath();
        
        // Fill the waveform
        g.fillPath(waveformPath);
    }
}

void SampleSlotComponent::resized()
{
    // Component fills its bounds
}

