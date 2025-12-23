#include "WaveformComponent.h"
#include <algorithm>
#include <cmath>

WaveformComponent::WaveformComponent() {
    // White waveform on dark background
    waveformColor = juce::Colours::white;
    backgroundColor = juce::Colour(0xFF0A0A0A); // Very dark background
    gridColor = juce::Colour(0xFF222222); // Subtle grid
    markerColor = juce::Colours::yellow; // Yellow markers for start/end points
    
    // Initialize sample editing parameters
    startPoint = 0;
    endPoint = 0;
    sampleGain = 1.0f;
    
    // Initialize loop parameters
    loopStartPoint = 0;
    loopEndPoint = 0;
    loopEnabled = false;
    
    // Initialize playhead
    playheadPosition = -1.0;
    envelopeValue = 0.0f;
    
    // Initialize playhead color (faint yellow)
    playheadColor = juce::Colour(0x80FFFF00); // Yellow with ~50% opacity
    
    setOpaque(true);
}

WaveformComponent::~WaveformComponent() {
}

void WaveformComponent::setSampleData(const std::vector<float>& data) {
    sampleData = data;
    // Initialize end point to full sample length
    if (endPoint == 0 && !data.empty()) {
        endPoint = static_cast<int>(data.size());
    }
    repaint();
}

void WaveformComponent::setStartPoint(int sampleIndex) {
    startPoint = sampleIndex;
    repaint();
}

void WaveformComponent::setEndPoint(int sampleIndex) {
    endPoint = sampleIndex;
    repaint();
}

void WaveformComponent::setSampleGain(float gain) {
    sampleGain = gain;
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
    
    // Calculate visible sample range (zoom to start/end points)
    int visibleStart = std::max(0, startPoint);
    int visibleEnd = std::min(static_cast<int>(sampleData.size()), endPoint);
    int visibleLength = visibleEnd - visibleStart;
    
    if (visibleLength <= 0) {
        return; // No visible range
    }
    
    // Higher resolution for smoother display (2x for smoother curves)
    int displayPoints = width * 2;
    int samplesPerPoint = static_cast<int>(std::ceil(static_cast<double>(visibleLength) / static_cast<double>(displayPoints)));
    
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
        int sampleOffset = x * samplesPerPoint;
        int startSample = visibleStart + sampleOffset;
        int endSample = std::min(startSample + samplesPerPoint, visibleEnd);
        
        if (startSample >= visibleEnd) {
            break;
        }
        
        // Find min and max in this range (OP-1 style)
        float minVal = sampleData[startSample];
        float maxVal = sampleData[startSample];
        
        for (int i = startSample + 1; i < endSample; ++i) {
            if (i < static_cast<int>(sampleData.size())) {
                minVal = std::min(minVal, sampleData[i]);
                maxVal = std::max(maxVal, sampleData[i]);
            }
        }
        
        // Convert to screen coordinates (scale X to fit bounds width)
        float xPos = static_cast<float>(bounds.getX()) + (static_cast<float>(x) / static_cast<float>(displayPoints)) * static_cast<float>(width);
        
        // Apply sample gain to visual scaling
        float scaledHalfHeight = halfHeight * sampleGain;
        
        // Calculate Y positions (inverted: -1.0 is bottom, 1.0 is top)
        float topY = centerY - (maxVal * scaledHalfHeight);
        float bottomY = centerY - (minVal * scaledHalfHeight);
        
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
    
    // Draw loop markers (white vertical lines) if loop is enabled
    if (loopEnabled && sampleData.size() > 0) {
        g.setColour(juce::Colours::white);
        
        // Calculate positions for loop markers
        int totalSamples = static_cast<int>(sampleData.size());
        if (totalSamples > 0) {
            // Calculate visible sample range
            int visibleStart = std::max(0, startPoint);
            int visibleEnd = std::min(totalSamples, endPoint);
            int visibleLength = visibleEnd - visibleStart;
            
            // Check if reverse loop is active (loopStartPoint > loopEndPoint)
            bool isReverseLoop = loopStartPoint > loopEndPoint;
            
            // Calculate line height (60% of bounds height - 40% shorter)
            float lineHeight = static_cast<float>(bounds.getHeight()) * 0.6f;
            float lineTop = static_cast<float>(bounds.getY()) + (static_cast<float>(bounds.getHeight()) - lineHeight) * 0.5f;
            float lineBottom = lineTop + lineHeight;
            
            // Set font for flags
            g.setFont(10.0f);
            
            if (visibleLength > 0) {
                // Draw loop start marker
                if (loopStartPoint >= visibleStart && loopStartPoint < visibleEnd) {
                    float loopStartX = static_cast<float>(bounds.getX()) + 
                        ((static_cast<float>(loopStartPoint - visibleStart) / static_cast<float>(visibleLength)) * static_cast<float>(width));
                    int startXInt = static_cast<int>(loopStartX);
                    
                    // Draw shorter vertical line (60% height)
                    g.drawVerticalLine(startXInt, lineTop, lineBottom);
                    
                    // Draw "S" flag as triangle pointer attached to top of line
                    // In reverse mode, triangle points left; otherwise points right
                    float flagSize = 16.0f;  // Bigger flag
                    float flagY = lineTop;   // Attached to top of line
                    juce::Path flagPath;
                    
                    if (isReverseLoop) {
                        // Triangle pointing left, back edge aligned with line
                        flagPath.addTriangle(loopStartX - flagSize, flagY + flagSize * 0.5f,  // Left point (apex)
                                           loopStartX, flagY,                                  // Top right (back edge on line)
                                           loopStartX, flagY + flagSize);                      // Bottom right (back edge on line)
                    } else {
                        // Triangle pointing right, back edge aligned with line
                        flagPath.addTriangle(loopStartX + flagSize, flagY + flagSize * 0.5f,  // Right point (apex)
                                           loopStartX, flagY,                                   // Top left (back edge on line)
                                           loopStartX, flagY + flagSize);                       // Bottom left (back edge on line)
                    }
                    
                    // Draw white triangle flag
                    g.setColour(juce::Colours::white);
                    g.fillPath(flagPath);
                    
                    // Draw "S" letter in black on the flag (bold font), positioned towards the back
                    g.setColour(juce::Colours::black);
                    juce::Font boldFont(10.0f, juce::Font::bold);
                    g.setFont(boldFont);
                    // Position text more towards the back (base) of the triangle
                    float textOffset = isReverseLoop ? -flagSize * 0.3f : flagSize * 0.3f;
                    juce::Rectangle<float> textRect(loopStartX + textOffset - flagSize * 0.25f, flagY, flagSize * 0.5f, flagSize);
                    g.drawText("S", textRect, juce::Justification::centred);
                    
                    // Reset color to white for the end line
                    g.setColour(juce::Colours::white);
                }
                
                // Draw loop end marker
                if (loopEndPoint > visibleStart && loopEndPoint <= visibleEnd) {
                    float loopEndX = static_cast<float>(bounds.getX()) + 
                        ((static_cast<float>(loopEndPoint - visibleStart) / static_cast<float>(visibleLength)) * static_cast<float>(width));
                    int endXInt = static_cast<int>(loopEndX);
                    
                    // Draw shorter vertical line (60% height)
                    g.drawVerticalLine(endXInt, lineTop, lineBottom);
                    
                    // Draw "E" flag as triangle pointer attached to top of line
                    // In reverse mode, triangle points right; otherwise points left
                    float flagSize = 16.0f;  // Bigger flag
                    float flagY = lineTop;    // Attached to top of line
                    juce::Path flagPath;
                    
                    if (isReverseLoop) {
                        // Triangle pointing right, back edge aligned with line
                        flagPath.addTriangle(loopEndX + flagSize, flagY + flagSize * 0.5f,  // Right point (apex)
                                           loopEndX, flagY,                                   // Top left (back edge on line)
                                           loopEndX, flagY + flagSize);                       // Bottom left (back edge on line)
                    } else {
                        // Triangle pointing left, back edge aligned with line
                        flagPath.addTriangle(loopEndX - flagSize, flagY + flagSize * 0.5f,  // Left point (apex)
                                           loopEndX, flagY,                                  // Top right (back edge on line)
                                           loopEndX, flagY + flagSize);                      // Bottom right (back edge on line)
                    }
                    
                    // Draw white triangle flag
                    g.setColour(juce::Colours::white);
                    g.fillPath(flagPath);
                    
                    // Draw "E" letter in black on the flag (bold font), positioned towards the back
                    g.setColour(juce::Colours::black);
                    juce::Font boldFont(10.0f, juce::Font::bold);
                    g.setFont(boldFont);
                    // Position text more towards the back (base) of the triangle
                    float textOffset = isReverseLoop ? flagSize * 0.3f : -flagSize * 0.3f;
                    juce::Rectangle<float> textRect(loopEndX + textOffset - flagSize * 0.25f, flagY, flagSize * 0.5f, flagSize);
                    g.drawText("E", textRect, juce::Justification::centred);
                }
            }
        }
    }
}

void WaveformComponent::setLoopStartPoint(int sampleIndex) {
    loopStartPoint = sampleIndex;
    repaint();
}

void WaveformComponent::setLoopEndPoint(int sampleIndex) {
    loopEndPoint = sampleIndex;
    repaint();
}

void WaveformComponent::setLoopEnabled(bool enabled) {
    loopEnabled = enabled;
    repaint();
}

void WaveformComponent::setPlayheadPosition(double sampleIndex, float envValue) {
    playheadPosition = sampleIndex;
    envelopeValue = envValue;
    repaint();
}

void WaveformComponent::resized() {
    // Component will repaint automatically
}

