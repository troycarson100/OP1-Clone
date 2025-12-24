#include "ADSRPillComponent.h"

ADSRPillComponent::ADSRPillComponent()
    : attackMs(2.0f)
    , decayMs(0.0f)
    , sustainLevel(1.0f)
    , releaseMs(20.0f)
{
    setOpaque(false);  // Transparent background (we draw the pill ourselves)
    setInterceptsMouseClicks(false, false);  // Don't intercept mouse clicks
}

ADSRPillComponent::~ADSRPillComponent()
{
}

void ADSRPillComponent::setADSR(float attack, float decay, float sustain, float release)
{
    attackMs = attack;
    decayMs = decay;
    sustainLevel = sustain;
    releaseMs = release;
    repaint();
}

float ADSRPillComponent::timeToX(float timeMs, float width) const
{
    // Map time to x coordinate within the pill
    // Use a fixed max time range for display (e.g., 1000ms = 1 second)
    const float maxTimeMs = 1000.0f;
    float normalizedTime = juce::jlimit(0.0f, 1.0f, timeMs / maxTimeMs);
    return normalizedTime * width;
}

float ADSRPillComponent::amplitudeToY(float amplitude, float height) const
{
    // Invert y: amplitude 1.0 = top (y=0), amplitude 0.0 = bottom (y=height)
    return height * (1.0f - amplitude);
}

void ADSRPillComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    float width = static_cast<float>(bounds.getWidth());
    float height = static_cast<float>(bounds.getHeight());
    
    if (width <= 0 || height <= 0) {
        return;
    }
    
    // Draw pill-shaped background (white)
    float cornerRadius = height * 0.5f;  // Pill shape: height/2 radius
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(bounds.toFloat(), cornerRadius);
    
    // Draw dark border
    g.setColour(juce::Colour(0xFF333333));
    g.drawRoundedRectangle(bounds.toFloat(), cornerRadius, 1.0f);
    
    // Calculate envelope bounds (with padding inside pill)
    float padding = 5.0f;  // Increased padding for better visual spacing
    float envelopeWidth = width - (padding * 2.0f);
    float envelopeHeight = height - (padding * 2.0f);
    float envelopeX = padding;
    float envelopeY = padding;
    
    // Use local copies to avoid modifying member variables
    float localAttack = attackMs;
    float localDecay = decayMs;
    float localSustain = sustainLevel;
    float localRelease = releaseMs;
    
    // Ensure minimum values for visibility
    if (localAttack < 1.0f) localAttack = 1.0f;
    if (localDecay < 1.0f) localDecay = 1.0f;
    if (localRelease < 1.0f) localRelease = 1.0f;
    
    // Calculate total time for proportional scaling
    // Use maximum ranges to normalize the proportions
    float attackMaxMs = 10000.0f;
    float decayMaxMs = 20000.0f;
    float releaseMaxMs = 20000.0f;
    
    // Normalize each phase to 0-1 range based on their max values
    float normalizedAttack = juce::jlimit(0.0f, 1.0f, localAttack / attackMaxMs);
    float normalizedDecay = juce::jlimit(0.0f, 1.0f, localDecay / decayMaxMs);
    float normalizedRelease = juce::jlimit(0.0f, 1.0f, localRelease / releaseMaxMs);
    
    // Calculate total normalized time (sustain is always 1.0 for its span)
    float totalNormalized = normalizedAttack + normalizedDecay + 1.0f + normalizedRelease;
    
    // If total is 0, use default proportions
    if (totalNormalized <= 0.0f) {
        totalNormalized = 4.0f;  // Default: 1 + 1 + 1 + 1
        normalizedAttack = 1.0f;
        normalizedDecay = 1.0f;
        normalizedRelease = 1.0f;
    }
    
    // Calculate proportional spans that always fill the full width
    float attackSpan = (normalizedAttack / totalNormalized) * envelopeWidth;
    float decaySpan = (normalizedDecay / totalNormalized) * envelopeWidth;
    float sustainSpan = (1.0f / totalNormalized) * envelopeWidth;  // Sustain always uses 1.0
    float releaseSpan = (normalizedRelease / totalNormalized) * envelopeWidth;
    
    // Calculate x positions - always fill the full width
    float x0 = envelopeX;
    float x1 = x0 + attackSpan;
    float x2 = x1 + decaySpan;
    float x3 = x2 + sustainSpan;
    float x4 = x3 + releaseSpan;
    
    // Ensure we fill exactly to the right edge (account for rounding)
    float rightBound = envelopeX + envelopeWidth;
    x4 = rightBound;
    
    // Calculate y positions (inverted: top is amplitude 1.0, bottom is 0.0)
    float y0 = envelopeY + envelopeHeight; // Start at bottom (amplitude 0)
    float y1 = envelopeY; // Attack peak at top (amplitude 1.0)
    float y2 = envelopeY + (1.0f - localSustain) * envelopeHeight; // Decay to sustain level
    float y3 = y2; // Sustain holds at same level
    float y4 = envelopeY + envelopeHeight; // Release to bottom (amplitude 0)
    
    // Draw envelope path with dark color (visible on white background)
    juce::Path envelopePath;
    envelopePath.startNewSubPath(x0, y0);
    
    // Attack: curve up to peak
    float attackControlX = x0 + (x1 - x0) * 0.5f;
    float attackControlY = y0 - (y0 - y1) * 0.3f;
    envelopePath.quadraticTo(attackControlX, attackControlY, x1, y1);
    
    // Decay: curve down to sustain level
    float decayControlX = x1 + (x2 - x1) * 0.5f;
    float decayControlY = y1 + (y2 - y1) * 0.3f;
    envelopePath.quadraticTo(decayControlX, decayControlY, x2, y2);
    
    // Sustain: horizontal line
    envelopePath.lineTo(x3, y3);
    
    // Release: curve down to zero
    float releaseControlX = x3 + (x4 - x3) * 0.5f;
    float releaseControlY = y3 + (y4 - y3) * 0.3f;
    envelopePath.quadraticTo(releaseControlX, releaseControlY, x4, y4);
    
    // Draw the envelope path with darker blue color
    juce::Colour blueColor = juce::Colour(0xFF5FA3D1); // Darker blue
    g.setColour(blueColor);
    g.strokePath(envelopePath, juce::PathStrokeType(1.5f));
    
    // Draw control points as small blue circles
    g.setColour(blueColor);
    g.fillEllipse(x0 - 2.0f, y0 - 2.0f, 4.0f, 4.0f);
    g.fillEllipse(x1 - 2.0f, y1 - 2.0f, 4.0f, 4.0f);
    g.fillEllipse(x2 - 2.0f, y2 - 2.0f, 4.0f, 4.0f);
    g.fillEllipse(x3 - 2.0f, y3 - 2.0f, 4.0f, 4.0f);
    g.fillEllipse(x4 - 2.0f, y4 - 2.0f, 4.0f, 4.0f);
}

