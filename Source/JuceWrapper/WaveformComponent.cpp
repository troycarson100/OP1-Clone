#include "WaveformComponent.h"
#include <algorithm>
#include <cmath>

WaveformComponent::WaveformComponent() {
    // White waveform on dark background
    waveformColor = juce::Colours::white;
    backgroundColor = juce::Colour(0xFF0A0A0A); // Very dark background
    gridColor = juce::Colour(0xFF222222); // Subtle grid
    
    setOpaque(true);
}

WaveformComponent::~WaveformComponent() {
}

void WaveformComponent::setSampleData(const std::vector<float>& data) {
    sampleData = data;
    repaint();
}

void WaveformComponent::clear() {
    sampleData.clear();
    repaint();
}

void WaveformComponent::paint(juce::Graphics& g) {
    // Fill background
    g.fillAll(backgroundColor);
    
    auto bounds = getLocalBounds().reduced(2);
    
    // Draw subtle grid
    g.setColour(gridColor);
    int centerY = bounds.getCentreY();
    g.drawHorizontalLine(centerY, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
    
    // Draw waveform
    if (sampleData.size() > 0) {
        drawWaveform(g, bounds);
    } else {
        // No sample loaded - show placeholder
        g.setColour(juce::Colours::grey);
        g.setFont(12.0f);
        g.drawText("No sample loaded", bounds, juce::Justification::centred);
    }
}

void WaveformComponent::drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds) {
    if (sampleData.empty()) {
        return;
    }
    
    g.setColour(waveformColor);
    
    int width = bounds.getWidth();
    int height = bounds.getHeight();
    int centerY = bounds.getCentreY();
    
    // Higher resolution for smoother display (2x for smoother curves)
    int displayPoints = width * 2;
    int samplesPerPoint = static_cast<int>(std::ceil(static_cast<double>(sampleData.size()) / static_cast<double>(displayPoints)));
    
    if (samplesPerPoint < 1) {
        samplesPerPoint = 1;
    }
    
    // Build smooth filled path for waveform (top edge = max, bottom edge = min)
    juce::Path waveformPath;
    std::vector<float> topPoints;
    std::vector<float> bottomPoints;
    std::vector<float> xPositions;
    
    // 50% less magnification: reduce scale by 50%
    float halfHeight = static_cast<float>(height) * 0.5f * 0.5f;
    
    for (int x = 0; x < displayPoints; ++x) {
        int startSample = x * samplesPerPoint;
        int endSample = std::min(startSample + samplesPerPoint, static_cast<int>(sampleData.size()));
        
        if (startSample >= static_cast<int>(sampleData.size())) {
            break;
        }
        
        // Find min and max in this range (OP-1 style)
        float minVal = sampleData[startSample];
        float maxVal = sampleData[startSample];
        
        for (int i = startSample + 1; i < endSample; ++i) {
            minVal = std::min(minVal, sampleData[i]);
            maxVal = std::max(maxVal, sampleData[i]);
        }
        
        // Convert to screen coordinates (scale X to fit bounds width)
        float xPos = static_cast<float>(bounds.getX()) + (static_cast<float>(x) / static_cast<float>(displayPoints)) * static_cast<float>(width);
        
        // Calculate Y positions (inverted: -1.0 is bottom, 1.0 is top)
        float topY = centerY - (maxVal * halfHeight);
        float bottomY = centerY - (minVal * halfHeight);
        
        // Clamp to bounds
        topY = std::max(static_cast<float>(bounds.getY()), std::min(static_cast<float>(bounds.getBottom()), topY));
        bottomY = std::max(static_cast<float>(bounds.getY()), std::min(static_cast<float>(bounds.getBottom()), bottomY));
        
        xPositions.push_back(xPos);
        topPoints.push_back(topY);
        bottomPoints.push_back(bottomY);
    }
    
    // Build smooth path with higher resolution points
    // More points + anti-aliasing = smooth appearance
    if (!xPositions.empty()) {
        // Start at first top point
        waveformPath.startNewSubPath(xPositions[0], topPoints[0]);
        
        // Draw top edge (max values) - more points = smoother with anti-aliasing
        for (size_t i = 1; i < xPositions.size(); ++i) {
            waveformPath.lineTo(xPositions[i], topPoints[i]);
        }
        
        // Draw bottom edge (min values) going backward
        for (int i = static_cast<int>(xPositions.size()) - 1; i >= 0; --i) {
            waveformPath.lineTo(xPositions[i], bottomPoints[i]);
        }
        
        // Close the path
        waveformPath.closeSubPath();
        
        // Draw filled waveform with anti-aliasing for smooth appearance
        g.fillPath(waveformPath);
    }
}

void WaveformComponent::resized() {
    // Component will repaint automatically
}

