#include "OrbitVisualizationComponent.h"
#include <algorithm>
#include <cmath>

OrbitVisualizationComponent::OrbitVisualizationComponent()
    : orbitWeights{0.25f, 0.25f, 0.25f, 0.25f}
    , orbitShape(0)
    , orbitRateHz(1.0f)
    , orbitPhase(0.0f)
{
    slotLoaded.fill(false);
    setOpaque(true);
}

OrbitVisualizationComponent::~OrbitVisualizationComponent()
{
}

void OrbitVisualizationComponent::setSlotPreview(int slotIndex, const std::vector<float>& sampleData)
{
    if (slotIndex >= 0 && slotIndex < 4) {
        slotPreviewData[slotIndex] = sampleData;
        slotLoaded[slotIndex] = !sampleData.empty();
        repaint();
    }
}

void OrbitVisualizationComponent::setOrbitWeights(const std::array<float, 4>& weights)
{
    orbitWeights = weights;
    repaint();
}

void OrbitVisualizationComponent::setOrbitShape(int shape)
{
    orbitShape = shape;
    repaint();
}

void OrbitVisualizationComponent::setOrbitRate(float rateHz)
{
    orbitRateHz = rateHz;
    repaint();
}

void OrbitVisualizationComponent::setOrbitPhase(float phase)
{
    orbitPhase = phase;
    repaint();
}

void OrbitVisualizationComponent::setSlotLoaded(int slotIndex, bool loaded)
{
    if (slotIndex >= 0 && slotIndex < 4) {
        slotLoaded[slotIndex] = loaded;
        repaint();
    }
}

void OrbitVisualizationComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    auto bounds = getLocalBounds();
    
    // Draw 4 slots in corners
    drawSlotInCorner(g, 0, bounds);  // A - top left
    drawSlotInCorner(g, 1, bounds);  // B - top right
    drawSlotInCorner(g, 2, bounds);  // C - bottom right
    drawSlotInCorner(g, 3, bounds);  // D - bottom left
    
    // Draw orbit shape overlay
    drawOrbitShape(g, bounds);
}

void OrbitVisualizationComponent::resized()
{
    repaint();
}

void OrbitVisualizationComponent::drawSlotInCorner(juce::Graphics& g, int slotIndex, const juce::Rectangle<int>& bounds)
{
    auto cornerBounds = getCornerBounds(slotIndex, bounds);
    
    // Draw slot background
    bool isActive = orbitWeights[slotIndex] > 0.01f;
    juce::Colour bgColor = isActive ? juce::Colour(0xFF2A2A2A) : juce::Colour(0xFF1A1A1A);
    g.setColour(bgColor);
    g.fillRoundedRectangle(cornerBounds.toFloat(), 8.0f);
    
    // Draw border
    g.setColour(juce::Colour(0xFF444444));
    g.drawRoundedRectangle(cornerBounds.toFloat(), 8.0f, 2.0f);
    
    // Draw slot label (A-D)
    char slotLetter = static_cast<char>('A' + slotIndex);
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    auto labelBounds = cornerBounds.removeFromTop(20);
    g.drawText(juce::String::charToString(slotLetter), labelBounds, juce::Justification::centred);
    
    // Draw waveform preview if slot is loaded
    if (slotLoaded[slotIndex] && !slotPreviewData[slotIndex].empty()) {
        auto waveformBounds = cornerBounds.reduced(5);
        drawWaveformPreview(g, waveformBounds, slotPreviewData[slotIndex], orbitWeights[slotIndex]);
    } else {
        // Draw empty slot indicator
        g.setColour(juce::Colour(0xFF333333));
        g.setFont(12.0f);
        g.drawText("Empty", cornerBounds, juce::Justification::centred);
    }
    
    // Weight indicator removed (green lines) - user requested removal
}

void OrbitVisualizationComponent::drawWaveformPreview(juce::Graphics& g, const juce::Rectangle<int>& bounds, const std::vector<float>& data, float weight)
{
    if (data.empty() || bounds.getWidth() <= 0 || bounds.getHeight() <= 0) return;
    
    int width = bounds.getWidth();
    int height = bounds.getHeight();
    
    // Downsample to fit width
    std::vector<float> minVals(width, 0.0f);
    std::vector<float> maxVals(width, 0.0f);
    
    for (int x = 0; x < width; ++x) {
        int startIdx = static_cast<int>((static_cast<float>(x) / static_cast<float>(width)) * static_cast<float>(data.size()));
        int endIdx = static_cast<int>((static_cast<float>(x + 1) / static_cast<float>(width)) * static_cast<float>(data.size()));
        endIdx = std::min(endIdx, static_cast<int>(data.size()));
        
        if (startIdx < endIdx) {
            auto minMax = std::minmax_element(data.begin() + startIdx, data.begin() + endIdx);
            minVals[x] = *minMax.first;
            maxVals[x] = *minMax.second;
        }
    }
    
    // Find max absolute value for normalization
    float maxAbs = 0.0f;
    for (int i = 0; i < width; ++i) {
        maxAbs = std::max(maxAbs, std::abs(minVals[i]));
        maxAbs = std::max(maxAbs, std::abs(maxVals[i]));
    }
    
    if (maxAbs < 0.0001f) return;
    
    // Draw waveform (brightness based on weight)
    float alpha = 0.3f + weight * 0.7f;  // 30% to 100% opacity
    g.setColour(juce::Colours::white.withAlpha(alpha));
    
    int centerY = bounds.getCentreY();
    float scale = (height * 0.25f) / maxAbs;  // Use 25% of height (smaller waveforms)
    
    juce::Path waveformPath;
    waveformPath.startNewSubPath(static_cast<float>(bounds.getX()), static_cast<float>(centerY));
    
    for (int x = 0; x < width; ++x) {
        float xPos = static_cast<float>(bounds.getX() + x);
        float minY = centerY + minVals[x] * scale;
        float maxY = centerY + maxVals[x] * scale;
        
        if (x == 0) {
            waveformPath.startNewSubPath(xPos, minY);
        } else {
            waveformPath.lineTo(xPos, minY);
        }
    }
    
    // Draw max values going back
    for (int x = width - 1; x >= 0; --x) {
        float xPos = static_cast<float>(bounds.getX() + x);
        float maxY = centerY + maxVals[x] * scale;
        waveformPath.lineTo(xPos, maxY);
    }
    
    waveformPath.closeSubPath();
    g.fillPath(waveformPath);
}

void OrbitVisualizationComponent::drawOrbitShape(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    // Draw orbit shape connecting the 4 corners
    g.setColour(juce::Colour(0x55FFFFFF));  // Semi-transparent white
    
    // Reserve space at bottom for parameter displays (2 rows of 40px + spacing = ~90px)
    int paramDisplayAreaHeight = 90;
    auto availableBounds = bounds.withTrimmedBottom(paramDisplayAreaHeight);
    
    int availableCenterX = availableBounds.getCentreX();
    int availableCenterY = availableBounds.getCentreY();
    int availableRadius = std::min(availableBounds.getWidth(), availableBounds.getHeight()) / 3;
    
    // Get corner positions
    auto topLeft = getCornerBounds(0, bounds).getCentre();
    auto topRight = getCornerBounds(1, bounds).getCentre();
    auto bottomRight = getCornerBounds(2, bounds).getCentre();
    auto bottomLeft = getCornerBounds(3, bounds).getCentre();
    
    juce::Path orbitPath;
    
    switch (orbitShape) {
        case 0: {  // Circle
            // Draw circular path through corners
            orbitPath.startNewSubPath(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
            orbitPath.addCentredArc(static_cast<float>(availableCenterX), static_cast<float>(availableCenterY),
                                    static_cast<float>(availableRadius), static_cast<float>(availableRadius),
                                    0.0f, 0.0f, juce::MathConstants<float>::twoPi);
            orbitPath.closeSubPath();
            g.strokePath(orbitPath, juce::PathStrokeType(2.0f));
            break;
        }
        case 1: {  // PingPong
            // Draw ping-pong path: A -> B -> C -> D -> C -> B -> A
            orbitPath.startNewSubPath(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
            orbitPath.lineTo(static_cast<float>(topRight.x), static_cast<float>(topRight.y));
            orbitPath.lineTo(static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y));
            orbitPath.lineTo(static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y));
            orbitPath.lineTo(static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y));
            orbitPath.lineTo(static_cast<float>(topRight.x), static_cast<float>(topRight.y));
            orbitPath.lineTo(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
            g.strokePath(orbitPath, juce::PathStrokeType(2.0f));
            break;
        }
        case 2: {  // Corners
            // Draw lines connecting corners in sequence
            orbitPath.startNewSubPath(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
            orbitPath.lineTo(static_cast<float>(topRight.x), static_cast<float>(topRight.y));
            orbitPath.lineTo(static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y));
            orbitPath.lineTo(static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y));
            orbitPath.closeSubPath();
            g.strokePath(orbitPath, juce::PathStrokeType(2.0f));
            break;
        }
        case 3: {  // RandomSmooth
            // Draw smooth curved path (simplified - just show a wavy path)
            orbitPath.startNewSubPath(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
            // Add some curves
            orbitPath.quadraticTo(static_cast<float>(availableCenterX), static_cast<float>(topLeft.y - 20),
                                  static_cast<float>(topRight.x), static_cast<float>(topRight.y));
            orbitPath.quadraticTo(static_cast<float>(availableCenterX + 20), static_cast<float>(availableCenterY),
                                  static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y));
            orbitPath.quadraticTo(static_cast<float>(availableCenterX), static_cast<float>(bottomRight.y + 20),
                                  static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y));
            orbitPath.quadraticTo(static_cast<float>(availableCenterX - 20), static_cast<float>(availableCenterY),
                                  static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
            g.strokePath(orbitPath, juce::PathStrokeType(2.0f));
            break;
        }
        case 4: {  // Figure8
            // Draw figure-8 (lemniscate) path
            float a = static_cast<float>(availableRadius) * 0.7f;
            int numPoints = 64;
            for (int i = 0; i <= numPoints; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(numPoints) * juce::MathConstants<float>::twoPi;
                float denom = 1.0f + std::sin(t) * std::sin(t);
                float x = a * std::cos(t) / denom;
                float y = a * std::sin(t) * std::cos(t) / denom;
                float px = static_cast<float>(availableCenterX) + x;
                float py = static_cast<float>(availableCenterY) + y;
                if (i == 0) {
                    orbitPath.startNewSubPath(px, py);
                } else {
                    orbitPath.lineTo(px, py);
                }
            }
            g.strokePath(orbitPath, juce::PathStrokeType(2.0f));
            break;
        }
        case 5: {  // ZigZag
            // Zig-zag: A -> C -> B -> D -> A
            orbitPath.startNewSubPath(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
            orbitPath.lineTo(static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y));
            orbitPath.lineTo(static_cast<float>(topRight.x), static_cast<float>(topRight.y));
            orbitPath.lineTo(static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y));
            orbitPath.closeSubPath();
            g.strokePath(orbitPath, juce::PathStrokeType(2.0f));
            break;
        }
        case 6: {  // Spiral
            // Spiral pattern - draw as expanding circle
            int numPoints = 32;
            for (int i = 0; i <= numPoints; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(numPoints) * juce::MathConstants<float>::twoPi;
                float r = availableRadius * (0.3f + 0.7f * static_cast<float>(i) / static_cast<float>(numPoints));
                float px = static_cast<float>(availableCenterX) + std::cos(t) * r;
                float py = static_cast<float>(availableCenterY) + std::sin(t) * r;
                if (i == 0) {
                    orbitPath.startNewSubPath(px, py);
                } else {
                    orbitPath.lineTo(px, py);
                }
            }
            g.strokePath(orbitPath, juce::PathStrokeType(2.0f));
            break;
        }
        case 7: {  // Square
            // Square/box: A -> B -> C -> D -> A (same as Corners but named differently)
            orbitPath.startNewSubPath(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
            orbitPath.lineTo(static_cast<float>(topRight.x), static_cast<float>(topRight.y));
            orbitPath.lineTo(static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y));
            orbitPath.lineTo(static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y));
            orbitPath.closeSubPath();
            g.strokePath(orbitPath, juce::PathStrokeType(2.0f));
            break;
        }
    }
    
    // Draw animated dot traveling along the orbit path
    if (orbitPhase >= 0.0f && orbitPhase <= 1.0f) {
        // Calculate dot position along the path based on phase
        float dotX, dotY;
        const float twoPi = juce::MathConstants<float>::twoPi;
        
        switch (orbitShape) {
            case 0: {  // Circle
                float angle = orbitPhase * twoPi;
                dotX = availableCenterX + std::cos(angle) * availableRadius;
                dotY = availableCenterY + std::sin(angle) * availableRadius;
                break;
            }
            case 1: {  // PingPong
                float p = orbitPhase * 6.0f;  // 0-6 for A->B->C->D->C->B->A
                int segment = static_cast<int>(p) % 6;
                float t = p - std::floor(p);
                
                juce::Point<float> start, end;
                switch (segment) {
                    case 0: start = {static_cast<float>(topLeft.x), static_cast<float>(topLeft.y)}; end = {static_cast<float>(topRight.x), static_cast<float>(topRight.y)}; break;
                    case 1: start = {static_cast<float>(topRight.x), static_cast<float>(topRight.y)}; end = {static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y)}; break;
                    case 2: start = {static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y)}; end = {static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y)}; break;
                    case 3: start = {static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y)}; end = {static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y)}; break;
                    case 4: start = {static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y)}; end = {static_cast<float>(topRight.x), static_cast<float>(topRight.y)}; break;
                    case 5: start = {static_cast<float>(topRight.x), static_cast<float>(topRight.y)}; end = {static_cast<float>(topLeft.x), static_cast<float>(topLeft.y)}; break;
                }
                dotX = start.x + (end.x - start.x) * t;
                dotY = start.y + (end.y - start.y) * t;
                break;
            }
            case 2: {  // Corners
                float p = orbitPhase * 4.0f;
                int corner = static_cast<int>(p) % 4;
                float t = p - std::floor(p);
                
                juce::Point<float> start, end;
                switch (corner) {
                    case 0: start = {static_cast<float>(topLeft.x), static_cast<float>(topLeft.y)}; end = {static_cast<float>(topRight.x), static_cast<float>(topRight.y)}; break;
                    case 1: start = {static_cast<float>(topRight.x), static_cast<float>(topRight.y)}; end = {static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y)}; break;
                    case 2: start = {static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y)}; end = {static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y)}; break;
                    case 3: start = {static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y)}; end = {static_cast<float>(topLeft.x), static_cast<float>(topLeft.y)}; break;
                }
                dotX = start.x + (end.x - start.x) * t;
                dotY = start.y + (end.y - start.y) * t;
                break;
            }
            case 3: {  // RandomSmooth - use circle as approximation
                float angle = orbitPhase * twoPi;
                dotX = availableCenterX + std::cos(angle) * availableRadius;
                dotY = availableCenterY + std::sin(angle) * availableRadius;
                break;
            }
            case 4: {  // Figure8
                float t = orbitPhase * twoPi;
                float a = static_cast<float>(availableRadius) * 0.7f;
                float denom = 1.0f + std::sin(t) * std::sin(t);
                dotX = availableCenterX + a * std::cos(t) / denom;
                dotY = availableCenterY + a * std::sin(t) * std::cos(t) / denom;
                break;
            }
            case 5: {  // ZigZag
                float p = orbitPhase * 4.0f;
                int segment = static_cast<int>(p) % 4;
                float t = p - std::floor(p);
                
                juce::Point<float> start, end;
                switch (segment) {
                    case 0: start = {static_cast<float>(topLeft.x), static_cast<float>(topLeft.y)}; end = {static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y)}; break;
                    case 1: start = {static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y)}; end = {static_cast<float>(topRight.x), static_cast<float>(topRight.y)}; break;
                    case 2: start = {static_cast<float>(topRight.x), static_cast<float>(topRight.y)}; end = {static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y)}; break;
                    case 3: start = {static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y)}; end = {static_cast<float>(topLeft.x), static_cast<float>(topLeft.y)}; break;
                }
                dotX = start.x + (end.x - start.x) * t;
                dotY = start.y + (end.y - start.y) * t;
                break;
            }
            case 6: {  // Spiral
                float t = orbitPhase * twoPi;
                float r = availableRadius * (0.3f + 0.7f * orbitPhase);
                dotX = availableCenterX + std::cos(t) * r;
                dotY = availableCenterY + std::sin(t) * r;
                break;
            }
            case 7: {  // Square
                float p = orbitPhase * 4.0f;
                int corner = static_cast<int>(p) % 4;
                float t = p - std::floor(p);
                
                juce::Point<float> start, end;
                switch (corner) {
                    case 0: start = {static_cast<float>(topLeft.x), static_cast<float>(topLeft.y)}; end = {static_cast<float>(topRight.x), static_cast<float>(topRight.y)}; break;
                    case 1: start = {static_cast<float>(topRight.x), static_cast<float>(topRight.y)}; end = {static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y)}; break;
                    case 2: start = {static_cast<float>(bottomRight.x), static_cast<float>(bottomRight.y)}; end = {static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y)}; break;
                    case 3: start = {static_cast<float>(bottomLeft.x), static_cast<float>(bottomLeft.y)}; end = {static_cast<float>(topLeft.x), static_cast<float>(topLeft.y)}; break;
                }
                dotX = start.x + (end.x - start.x) * t;
                dotY = start.y + (end.y - start.y) * t;
                break;
            }
            default:
                dotX = availableCenterX;
                dotY = availableCenterY;
                break;
        }
        
        // Draw the traveling dot (pink/magenta)
        float dotSize = 8.0f;
        g.setColour(juce::Colour(0xFFFF00FF));  // Magenta dot
        g.fillEllipse(dotX - dotSize/2, dotY - dotSize/2, dotSize, dotSize);
        
        // Draw a small white outline for visibility
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawEllipse(dotX - dotSize/2, dotY - dotSize/2, dotSize, dotSize, 1.5f);
    }
}

juce::Rectangle<int> OrbitVisualizationComponent::getCornerBounds(int slotIndex, const juce::Rectangle<int>& fullBounds)
{
    // Reserve space at bottom for parameter displays (2 rows of 40px + spacing = ~90px)
    int paramDisplayAreaHeight = 90;
    auto availableBounds = fullBounds.withTrimmedBottom(paramDisplayAreaHeight);
    
    int width = availableBounds.getWidth() / 2 - 10;
    int height = availableBounds.getHeight() / 2 - 10;
    
    switch (slotIndex) {
        case 0:  // Top left (A)
            return juce::Rectangle<int>(availableBounds.getX() + 5, availableBounds.getY() + 5, width, height);
        case 1:  // Top right (B)
            return juce::Rectangle<int>(availableBounds.getCentreX() + 5, availableBounds.getY() + 5, width, height);
        case 2:  // Bottom right (C)
            return juce::Rectangle<int>(availableBounds.getCentreX() + 5, availableBounds.getCentreY() + 5, width, height);
        case 3:  // Bottom left (D)
            return juce::Rectangle<int>(availableBounds.getX() + 5, availableBounds.getCentreY() + 5, width, height);
        default:
            return juce::Rectangle<int>();
    }
}

