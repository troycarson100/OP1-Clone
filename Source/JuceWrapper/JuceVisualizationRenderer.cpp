#include "JuceVisualizationRenderer.h"
#include <algorithm>
#include <cmath>

JuceVisualizationRenderer::JuceVisualizationRenderer()
    : graphics(nullptr)
{
}

JuceVisualizationRenderer::~JuceVisualizationRenderer()
{
}

void JuceVisualizationRenderer::setGraphicsContext(juce::Graphics* g)
{
    graphics = g;
}

juce::Colour JuceVisualizationRenderer::toJuceColor(const Core::Color& color) const
{
    return juce::Colour(color.r, color.g, color.b, color.a);
}

juce::Rectangle<int> JuceVisualizationRenderer::toJuceRectangle(const Core::Rectangle& rect) const
{
    return juce::Rectangle<int>(rect.x, rect.y, rect.width, rect.height);
}

void JuceVisualizationRenderer::fillBackground(const Core::Color& color, const Core::Rectangle& bounds)
{
    if (graphics == nullptr) return;
    
    graphics->setColour(toJuceColor(color));
    graphics->fillRect(toJuceRectangle(bounds));
}

void JuceVisualizationRenderer::renderWaveform(const Core::WaveformData& data, const Core::Rectangle& bounds)
{
    if (graphics == nullptr) return;
    
    // Note: Background should be filled by the component, not here
    // This allows the component to control the full background area
    
    // Check if we have stereo data
    bool isStereo = !data.leftChannel.empty() && !data.rightChannel.empty();
    
    if (isStereo) {
        // Stereo: Draw two waveforms stacked vertically with L/R labels
        // Make them closer together by using more of the height for each channel
        int channelHeight = static_cast<int>(bounds.height * 0.45f);  // 45% each
        int gap = static_cast<int>(bounds.height * 0.08f);  // 8% gap between channels
        int topOffset = static_cast<int>(bounds.height * 0.01f);  // 1% offset from top
        int topY = bounds.y + topOffset + 25;  // Left channel: move down 25px
        int bottomY = bounds.y + channelHeight + gap - 5;  // Right channel: move up 5px
        
        // Top: Left channel
        Core::Rectangle topBounds(bounds.x, topY, bounds.width, channelHeight);
        drawWaveformPathForChannel(data.leftChannel, data, topBounds);
        drawChannelLabel("L", topBounds);  // L label at bottom of waveform
        
        // Bottom: Right channel
        Core::Rectangle bottomBounds(bounds.x, bottomY, bounds.width, channelHeight);
        drawWaveformPathForChannel(data.rightChannel, data, bottomBounds);
        drawChannelLabel("R", bottomBounds);  // R label at bottom of waveform
        
        // Grid lines removed - no center lines through waveforms
        // graphics->setColour(toJuceColor(data.gridColor));
        // int topCenterY = topBounds.getCentreY();
        // int bottomCenterY = bottomBounds.getCentreY();
        // graphics->drawHorizontalLine(topCenterY, static_cast<float>(bounds.x), static_cast<float>(bounds.getRight()));
        // graphics->drawHorizontalLine(bottomCenterY, static_cast<float>(bounds.x), static_cast<float>(bounds.getRight()));
        
        // Draw loop markers if enabled (on both channels)
        if (data.loopEnabled) {
            drawLoopMarkersForChannel(data, topBounds);
            drawLoopMarkersForChannel(data, bottomBounds);
        }
    } else {
        // Mono: Draw single waveform centered
        const std::vector<float>* sampleData = nullptr;
        if (!data.leftChannel.empty()) {
            sampleData = &data.leftChannel;
        } else if (!data.rightChannel.empty()) {
            sampleData = &data.rightChannel;
        }

        if (sampleData == nullptr || sampleData->empty()) {
            // No sample - show placeholder
            graphics->setColour(juce::Colours::grey);
            graphics->setFont(12.0f);
            juce::Rectangle<int> textBounds(bounds.x, bounds.y, bounds.width, bounds.height);
            graphics->drawText("No sample loaded", textBounds, juce::Justification::centred);
            return;
        }

        // Grid line removed - no center line through waveform
        // graphics->setColour(toJuceColor(data.gridColor));
        // int centerY = bounds.getCentreY();
        // graphics->drawHorizontalLine(centerY, static_cast<float>(bounds.x), static_cast<float>(bounds.getRight()));

        // Draw waveform
        drawWaveformPath(data, bounds);

        // Draw loop markers if enabled
        if (data.loopEnabled && !sampleData->empty()) {
            drawLoopMarkers(data, bounds);
        }
    }
    
    // COMMENTED OUT: Draw all playhead lines (one per active voice)
    // if (!data.playheads.empty() && !sampleData->empty()) {
    //     drawPlayheads(data, bounds);
    // }
}

void JuceVisualizationRenderer::drawWaveformPath(const Core::WaveformData& data, const Core::Rectangle& bounds)
{
    if (graphics == nullptr) return;
    
    const std::vector<float>* sampleData = nullptr;
    if (!data.leftChannel.empty()) {
        sampleData = &data.leftChannel;
    } else if (!data.rightChannel.empty()) {
        sampleData = &data.rightChannel;
    }
    
    if (sampleData == nullptr || sampleData->empty()) return;
    
    graphics->setColour(toJuceColor(data.waveformColor));
    
    int width = bounds.width;
    int height = bounds.height;
    int centerY = bounds.getCentreY();
    
    // Calculate visible sample range
    int visibleStart = std::max(0, data.startPoint);
    int visibleEnd = std::min(static_cast<int>(sampleData->size()), data.endPoint);
    int visibleLength = visibleEnd - visibleStart;
    
    if (visibleLength <= 0) return;
    
    // Higher resolution for smoother display
    int displayPoints = width * 2;
    int samplesPerPoint = static_cast<int>(std::ceil(static_cast<double>(visibleLength) / static_cast<double>(displayPoints)));
    
    if (samplesPerPoint < 1) {
        samplesPerPoint = 1;
    }
    
    // Build smooth filled path
    juce::Path waveformPath;
    std::vector<float> topPoints;
    std::vector<float> bottomPoints;
    std::vector<float> xPositions;
    
    float halfHeight = static_cast<float>(height) * 0.5f * 0.5f;  // 50% less magnification
    
    for (int x = 0; x < displayPoints; ++x) {
        int sampleOffset = x * samplesPerPoint;
        int startSample = visibleStart + sampleOffset;
        int endSample = std::min(startSample + samplesPerPoint, visibleEnd);
        
        if (startSample >= visibleEnd) {
            break;
        }
        
        // Find min and max in this range
        float minVal = (*sampleData)[startSample];
        float maxVal = (*sampleData)[startSample];
        
        for (int i = startSample + 1; i < endSample; ++i) {
            if (i < static_cast<int>(sampleData->size())) {
                minVal = std::min(minVal, (*sampleData)[i]);
                maxVal = std::max(maxVal, (*sampleData)[i]);
            }
        }
        
        // Convert to screen coordinates
        float xPos = static_cast<float>(bounds.x) + (static_cast<float>(x) / static_cast<float>(displayPoints)) * static_cast<float>(width);
        
        // Apply sample gain to visual scaling
        float scaledHalfHeight = halfHeight * data.sampleGain;
        
        // Calculate Y positions (inverted)
        float topY = centerY - (maxVal * scaledHalfHeight);
        float bottomY = centerY - (minVal * scaledHalfHeight);
        
        // Clamp to bounds
        topY = std::max(static_cast<float>(bounds.y), std::min(static_cast<float>(bounds.getBottom()), topY));
        bottomY = std::max(static_cast<float>(bounds.y), std::min(static_cast<float>(bounds.getBottom()), bottomY));
        
        xPositions.push_back(xPos);
        topPoints.push_back(topY);
        bottomPoints.push_back(bottomY);
    }
    
    // Build path
    if (!xPositions.empty()) {
        waveformPath.startNewSubPath(xPositions[0], topPoints[0]);
        
        for (size_t i = 1; i < xPositions.size(); ++i) {
            waveformPath.lineTo(xPositions[i], topPoints[i]);
        }
        
        for (int i = static_cast<int>(xPositions.size()) - 1; i >= 0; --i) {
            waveformPath.lineTo(xPositions[i], bottomPoints[i]);
        }
        
        waveformPath.closeSubPath();
        graphics->fillPath(waveformPath);
    }
}

void JuceVisualizationRenderer::drawLoopMarkers(const Core::WaveformData& data, const Core::Rectangle& bounds)
{
    if (graphics == nullptr) return;
    
    const std::vector<float>* sampleData = nullptr;
    if (!data.leftChannel.empty()) {
        sampleData = &data.leftChannel;
    } else if (!data.rightChannel.empty()) {
        sampleData = &data.rightChannel;
    }
    
    if (sampleData == nullptr || sampleData->empty()) return;
    
    graphics->setColour(toJuceColor(data.markerColor));
    
    int totalSamples = static_cast<int>(sampleData->size());
    if (totalSamples <= 0) return;
    
    int visibleStart = std::max(0, data.startPoint);
    int visibleEnd = std::min(totalSamples, data.endPoint);
    int visibleLength = visibleEnd - visibleStart;
    
    bool isReverseLoop = data.loopStartPoint > data.loopEndPoint;
    
    float lineHeight = static_cast<float>(bounds.height) * 0.6f;
    float lineTop = static_cast<float>(bounds.y) + (static_cast<float>(bounds.height) - lineHeight) * 0.5f;
    float lineBottom = lineTop + lineHeight;
    
    graphics->setFont(10.0f);
    
    if (visibleLength > 0) {
        int width = bounds.width;
        
        // Draw loop start marker
        if (data.loopStartPoint >= visibleStart && data.loopStartPoint < visibleEnd) {
            float loopStartX = static_cast<float>(bounds.x) + 
                ((static_cast<float>(data.loopStartPoint - visibleStart) / static_cast<float>(visibleLength)) * static_cast<float>(width));
            int startXInt = static_cast<int>(loopStartX);
            
            graphics->drawVerticalLine(startXInt, lineTop, lineBottom);
            
            // Draw "S" flag
            float flagSize = 16.0f;
            float flagY = lineTop;
            juce::Path flagPath;
            
            if (isReverseLoop) {
                flagPath.addTriangle(loopStartX - flagSize, flagY + flagSize * 0.5f,
                                   loopStartX, flagY,
                                   loopStartX, flagY + flagSize);
            } else {
                flagPath.addTriangle(loopStartX + flagSize, flagY + flagSize * 0.5f,
                                   loopStartX, flagY,
                                   loopStartX, flagY + flagSize);
            }
            
            graphics->setColour(toJuceColor(data.markerColor));
            graphics->fillPath(flagPath);
            
            graphics->setColour(juce::Colours::black);  // Black text
            juce::Font boldFont(10.0f, juce::Font::bold);
            graphics->setFont(boldFont);
            float textOffset = isReverseLoop ? -flagSize * 0.3f : flagSize * 0.3f;
            juce::Rectangle<float> textRect(loopStartX + textOffset - flagSize * 0.25f, flagY, flagSize * 0.5f, flagSize);
            graphics->drawText("S", textRect, juce::Justification::centred);
            
            graphics->setColour(toJuceColor(data.markerColor));
        }
        
        // Draw loop end marker
        if (data.loopEndPoint > visibleStart && data.loopEndPoint <= visibleEnd) {
            float loopEndX = static_cast<float>(bounds.x) + 
                ((static_cast<float>(data.loopEndPoint - visibleStart) / static_cast<float>(visibleLength)) * static_cast<float>(width));
            int endXInt = static_cast<int>(loopEndX);
            
            graphics->setColour(juce::Colours::white);  // White line for loop end
            graphics->drawVerticalLine(endXInt, lineTop, lineBottom);
            
            // Draw "E" flag
            float flagSize = 16.0f;
            float flagY = lineTop;
            juce::Path flagPath;
            
            if (isReverseLoop) {
                flagPath.addTriangle(loopEndX + flagSize, flagY + flagSize * 0.5f,
                                   loopEndX, flagY,
                                   loopEndX, flagY + flagSize);
            } else {
                flagPath.addTriangle(loopEndX - flagSize, flagY + flagSize * 0.5f,
                                   loopEndX, flagY,
                                   loopEndX, flagY + flagSize);
            }
            
            graphics->setColour(toJuceColor(data.markerColor));
            graphics->fillPath(flagPath);
            
            graphics->setColour(juce::Colours::black);  // Black text
            juce::Font boldFont(10.0f, juce::Font::bold);
            graphics->setFont(boldFont);
            float textOffset = isReverseLoop ? flagSize * 0.3f : -flagSize * 0.3f;
            juce::Rectangle<float> textRect(loopEndX + textOffset - flagSize * 0.25f, flagY, flagSize * 0.5f, flagSize);
            graphics->drawText("E", textRect, juce::Justification::centred);
        }
    }
}

void JuceVisualizationRenderer::drawPlayheads(const Core::WaveformData& data, const Core::Rectangle& bounds)
{
    if (graphics == nullptr) return;
    
    const std::vector<float>* sampleData = nullptr;
    if (!data.leftChannel.empty()) {
        sampleData = &data.leftChannel;
    } else if (!data.rightChannel.empty()) {
        sampleData = &data.rightChannel;
    }
    
    if (sampleData == nullptr || sampleData->empty()) return;
    
    int visibleStart = std::max(0, data.startPoint);
    int visibleEnd = std::min(static_cast<int>(sampleData->size()), data.endPoint);
    int visibleLength = visibleEnd - visibleStart;
    
    if (visibleLength <= 0) return;
    
    // Draw a playhead line for each active voice with fade in/out
    for (const auto& playhead : data.playheads) {
        // Only draw if envelope value indicates active playback
        if (playhead.envelopeValue <= 0.0f) continue;
        
        if (playhead.position >= visibleStart && playhead.position < visibleEnd) {
            float playheadX = static_cast<float>(bounds.x) + 
                ((static_cast<float>(playhead.position - visibleStart) / static_cast<float>(visibleLength)) * static_cast<float>(bounds.width));
            
            // Fade in/out based on envelope value (multiply base opacity by envelope)
            // Envelope goes from 0.0 (silent) to 1.0 (full volume)
            Core::Color baseColor = data.playheadColor;
            uint8_t fadedAlpha = static_cast<uint8_t>(baseColor.a * playhead.envelopeValue);
            Core::Color fadedColor(baseColor.r, baseColor.g, baseColor.b, fadedAlpha);
            
            graphics->setColour(toJuceColor(fadedColor));
            
            // Draw smooth vertical line (using anti-aliasing)
            graphics->drawVerticalLine(static_cast<int>(playheadX), static_cast<float>(bounds.y), static_cast<float>(bounds.getBottom()));
        }
    }
}

void JuceVisualizationRenderer::renderADSR(const Core::ADSRData& data, const Core::Rectangle& bounds)
{
    if (graphics == nullptr) return;
    
    drawADSREnvelope(data, bounds);
}

void JuceVisualizationRenderer::drawADSREnvelope(const Core::ADSRData& data, const Core::Rectangle& bounds)
{
    if (graphics == nullptr) return;
    
    float width = static_cast<float>(bounds.width);
    float height = static_cast<float>(bounds.height);
    
    if (width <= 0 || height <= 0) {
        return;
    }
    
    // Draw pill-shaped background
    float cornerRadius = height * 0.5f;
    graphics->setColour(toJuceColor(data.backgroundColor));
    graphics->fillRoundedRectangle(bounds.x, bounds.y, width, height, cornerRadius);
    
    // Draw border
    graphics->setColour(toJuceColor(data.borderColor));
    graphics->drawRoundedRectangle(bounds.x, bounds.y, width, height, cornerRadius, 1.0f);
    
    // Calculate envelope bounds (with padding)
    float horizontalPadding = 8.0f;  // Horizontal padding (left/right) - keep same
    float verticalPadding = 4.0f;     // Vertical padding (top/bottom) - reduced for taller lines
    float envelopeWidth = width - (horizontalPadding * 2.0f);
    float envelopeHeight = height - (verticalPadding * 2.0f);
    float envelopeX = static_cast<float>(bounds.x) + horizontalPadding;
    float envelopeY = static_cast<float>(bounds.y) + verticalPadding;
    
    // Use local copies
    float localAttack = std::max(1.0f, data.attackMs);
    float localDecay = std::max(1.0f, data.decayMs);
    float localSustain = data.sustainLevel;
    float localRelease = std::max(1.0f, data.releaseMs);
    
    // Normalize for proportional scaling
    float attackMaxMs = 10000.0f;
    float decayMaxMs = 20000.0f;
    float releaseMaxMs = 20000.0f;
    
    float normalizedAttack = std::max(0.0f, std::min(1.0f, localAttack / attackMaxMs));
    float normalizedDecay = std::max(0.0f, std::min(1.0f, localDecay / decayMaxMs));
    float normalizedRelease = std::max(0.0f, std::min(1.0f, localRelease / releaseMaxMs));
    
    float totalNormalized = normalizedAttack + normalizedDecay + 1.0f + normalizedRelease;
    
    if (totalNormalized <= 0.0f) {
        totalNormalized = 4.0f;
        normalizedAttack = normalizedDecay = normalizedRelease = 1.0f;
    }
    
    // Calculate proportional spans
    float attackSpan = (normalizedAttack / totalNormalized) * envelopeWidth;
    float decaySpan = (normalizedDecay / totalNormalized) * envelopeWidth;
    float sustainSpan = (1.0f / totalNormalized) * envelopeWidth;
    float releaseSpan = (normalizedRelease / totalNormalized) * envelopeWidth;
    
    // Calculate x positions
    float x0 = envelopeX;
    float x1 = x0 + attackSpan;
    float x2 = x1 + decaySpan;
    float x3 = x2 + sustainSpan;
    float x4 = envelopeX + envelopeWidth;  // Always fill to right edge
    
    // Calculate y positions
    float y0 = envelopeY + envelopeHeight;
    float y1 = envelopeY;
    float y2 = envelopeY + (1.0f - localSustain) * envelopeHeight;
    float y3 = y2;
    float y4 = envelopeY + envelopeHeight;
    
    // Draw envelope path
    juce::Path envelopePath;
    envelopePath.startNewSubPath(x0, y0);
    
    float attackControlX = x0 + (x1 - x0) * 0.5f;
    float attackControlY = y0 - (y0 - y1) * 0.3f;
    envelopePath.quadraticTo(attackControlX, attackControlY, x1, y1);
    
    float decayControlX = x1 + (x2 - x1) * 0.5f;
    float decayControlY = y1 + (y2 - y1) * 0.3f;
    envelopePath.quadraticTo(decayControlX, decayControlY, x2, y2);
    
    envelopePath.lineTo(x3, y3);
    
    float releaseControlX = x3 + (x4 - x3) * 0.5f;
    float releaseControlY = y3 + (y4 - y3) * 0.3f;
    envelopePath.quadraticTo(releaseControlX, releaseControlY, x4, y4);
    
    // Draw envelope
    graphics->setColour(toJuceColor(data.envelopeColor));
    graphics->strokePath(envelopePath, juce::PathStrokeType(1.5f));
    
    // Draw control points
    graphics->setColour(toJuceColor(data.envelopeColor));
    graphics->fillEllipse(x0 - 2.0f, y0 - 2.0f, 4.0f, 4.0f);
    graphics->fillEllipse(x1 - 2.0f, y1 - 2.0f, 4.0f, 4.0f);
    graphics->fillEllipse(x2 - 2.0f, y2 - 2.0f, 4.0f, 4.0f);
    graphics->fillEllipse(x3 - 2.0f, y3 - 2.0f, 4.0f, 4.0f);
    graphics->fillEllipse(x4 - 2.0f, y4 - 2.0f, 4.0f, 4.0f);
}

void JuceVisualizationRenderer::drawWaveformPathForChannel(const std::vector<float>& channelData, const Core::WaveformData& data, const Core::Rectangle& bounds)
{
    if (graphics == nullptr || channelData.empty()) return;

    graphics->setColour(toJuceColor(data.waveformColor));

    int width = bounds.width;
    int height = bounds.height;
    int centerY = bounds.getCentreY();

    // Calculate visible sample range
    int visibleStart = std::max(0, data.startPoint);
    int visibleEnd = std::min(static_cast<int>(channelData.size()), data.endPoint);
    int visibleLength = visibleEnd - visibleStart;

    if (visibleLength <= 0) return;

    // Higher resolution for smoother display
    int displayPoints = width * 2;
    int samplesPerPoint = static_cast<int>(std::ceil(static_cast<double>(visibleLength) / static_cast<double>(displayPoints)));

    if (samplesPerPoint < 1) {
        samplesPerPoint = 1;
    }

    // Build smooth filled path
    juce::Path waveformPath;
    std::vector<float> topPoints;
    std::vector<float> bottomPoints;
    std::vector<float> xPositions;

    float halfHeight = static_cast<float>(height) * 0.5f * 0.5f;  // 50% less magnification

    for (int x = 0; x < displayPoints; ++x) {
        int sampleOffset = x * samplesPerPoint;
        int startSample = visibleStart + sampleOffset;
        int endSample = std::min(startSample + samplesPerPoint, visibleEnd);

        if (startSample >= visibleEnd) {
            break;
        }

        // Find min and max in this range
        float minVal = channelData[startSample];
        float maxVal = channelData[startSample];

        for (int i = startSample + 1; i < endSample; ++i) {
            if (i < static_cast<int>(channelData.size())) {
                minVal = std::min(minVal, channelData[i]);
                maxVal = std::max(maxVal, channelData[i]);
            }
        }

        // Convert to screen coordinates
        float xPos = static_cast<float>(bounds.x) + (static_cast<float>(x) / static_cast<float>(displayPoints)) * static_cast<float>(width);

        // Apply sample gain to visual scaling
        float scaledHalfHeight = halfHeight * data.sampleGain;

        // Calculate Y positions (inverted)
        float topY = centerY - (maxVal * scaledHalfHeight);
        float bottomY = centerY - (minVal * scaledHalfHeight);

        // Clamp to bounds
        topY = std::max(static_cast<float>(bounds.y), std::min(static_cast<float>(bounds.getBottom()), topY));
        bottomY = std::max(static_cast<float>(bounds.y), std::min(static_cast<float>(bounds.getBottom()), bottomY));

        xPositions.push_back(xPos);
        topPoints.push_back(topY);
        bottomPoints.push_back(bottomY);
    }

    // Build path
    if (!xPositions.empty()) {
        waveformPath.startNewSubPath(xPositions[0], topPoints[0]);

        for (size_t i = 1; i < xPositions.size(); ++i) {
            waveformPath.lineTo(xPositions[i], topPoints[i]);
        }

        for (int i = static_cast<int>(xPositions.size()) - 1; i >= 0; --i) {
            waveformPath.lineTo(xPositions[i], bottomPoints[i]);
        }

        waveformPath.closeSubPath();
        graphics->fillPath(waveformPath);
    }
}

void JuceVisualizationRenderer::drawLoopMarkersForChannel(const Core::WaveformData& data, const Core::Rectangle& bounds)
{
    if (graphics == nullptr) return;

    const std::vector<float>* sampleData = nullptr;
    if (!data.leftChannel.empty()) {
        sampleData = &data.leftChannel;
    } else if (!data.rightChannel.empty()) {
        sampleData = &data.rightChannel;
    }

    if (sampleData == nullptr || sampleData->empty()) return;

    graphics->setColour(toJuceColor(data.markerColor));

    int totalSamples = static_cast<int>(sampleData->size());
    if (totalSamples <= 0) return;

    int width = bounds.width;
    int visibleStart = std::max(0, data.startPoint);
    int visibleEnd = std::min(totalSamples, data.endPoint);
    int visibleLength = visibleEnd - visibleStart;

    if (visibleLength <= 0) return;

    bool isReverseLoop = data.loopStartPoint > data.loopEndPoint;

    float lineHeight = static_cast<float>(bounds.height) * 0.6f;
    float lineTop = static_cast<float>(bounds.y) + (static_cast<float>(bounds.height) - lineHeight) * 0.5f;
    float lineBottom = lineTop + lineHeight;

    graphics->setFont(10.0f);

    // Draw loop start marker
    if (data.loopStartPoint >= visibleStart && data.loopStartPoint < visibleEnd) {
        float loopStartX = static_cast<float>(bounds.x) +
            ((static_cast<float>(data.loopStartPoint - visibleStart) / static_cast<float>(visibleLength)) * static_cast<float>(width));
        int startXInt = static_cast<int>(loopStartX);

        graphics->drawVerticalLine(startXInt, lineTop, lineBottom);

        float flagSize = 16.0f;
        float flagY = lineTop;
        juce::Path flagPath;

        if (isReverseLoop) {
            flagPath.addTriangle(loopStartX - flagSize, flagY + flagSize * 0.5f,
                               loopStartX, flagY,
                               loopStartX, flagY + flagSize);
        } else {
            flagPath.addTriangle(loopStartX + flagSize, flagY + flagSize * 0.5f,
                               loopStartX, flagY,
                               loopStartX, flagY + flagSize);
        }

        graphics->setColour(toJuceColor(data.markerColor));
        graphics->fillPath(flagPath);

        graphics->setColour(juce::Colours::black);  // Black text
        juce::Font boldFont(10.0f, juce::Font::bold);
        graphics->setFont(boldFont);
        float textOffset = isReverseLoop ? -flagSize * 0.3f : flagSize * 0.3f;
        juce::Rectangle<float> textRect(loopStartX + textOffset - flagSize * 0.25f, flagY, flagSize * 0.5f, flagSize);
        graphics->drawText("S", textRect, juce::Justification::centred);
    }

    // Draw loop end marker
    if (data.loopEndPoint > visibleStart && data.loopEndPoint <= visibleEnd) {
        float loopEndX = static_cast<float>(bounds.x) +
            ((static_cast<float>(data.loopEndPoint - visibleStart) / static_cast<float>(visibleLength)) * static_cast<float>(width));
        int endXInt = static_cast<int>(loopEndX);

        graphics->setColour(juce::Colours::white);  // White line for loop end
        graphics->drawVerticalLine(endXInt, lineTop, lineBottom);

        float flagSize = 16.0f;
        float flagY = lineTop;
        juce::Path flagPath;

        if (isReverseLoop) {
            flagPath.addTriangle(loopEndX + flagSize, flagY + flagSize * 0.5f,
                               loopEndX, flagY,
                               loopEndX, flagY + flagSize);
        } else {
            flagPath.addTriangle(loopEndX - flagSize, flagY + flagSize * 0.5f,
                               loopEndX, flagY,
                               loopEndX, flagY + flagSize);
        }

        graphics->setColour(toJuceColor(data.markerColor));
        graphics->fillPath(flagPath);

        graphics->setColour(juce::Colours::black);  // Black text
        juce::Font boldFont(10.0f, juce::Font::bold);
        graphics->setFont(boldFont);
        float textOffset = isReverseLoop ? flagSize * 0.3f : -flagSize * 0.3f;
        juce::Rectangle<float> textRect(loopEndX + textOffset - flagSize * 0.25f, flagY, flagSize * 0.5f, flagSize);
        graphics->drawText("E", textRect, juce::Justification::centred);
    }
}

void JuceVisualizationRenderer::drawChannelLabel(const char* label, const Core::Rectangle& bounds)
{
    if (graphics == nullptr) return;

    // Position label on the left side, centered vertically
    float circleRadius = 7.0f;  // Smaller circle
    float circleX = static_cast<float>(bounds.x) + circleRadius + 2.0f;  // 2px from left edge (moved left)
    float circleY = static_cast<float>(bounds.getCentreY()) + 20.0f;  // Centered vertically, moved down 20px
    
    // Draw black circle background
    graphics->setColour(juce::Colours::black);
    graphics->fillEllipse(circleX - circleRadius, circleY - circleRadius, circleRadius * 2.0f, circleRadius * 2.0f);
    
    // Outline removed - no border
    
    // Draw white text centered in circle (keep font size the same)
    graphics->setColour(juce::Colours::white);
    juce::Font boldFont(10.0f, juce::Font::bold);
    graphics->setFont(boldFont);
    juce::Rectangle<float> textBounds(circleX - circleRadius, circleY - circleRadius, circleRadius * 2.0f, circleRadius * 2.0f);
    graphics->drawText(label, textBounds, juce::Justification::centred);
}

