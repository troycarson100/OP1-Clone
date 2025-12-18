#include "ADSRVisualizationComponent.h"

ADSRVisualizationComponent::ADSRVisualizationComponent()
    : attackMs(2.0f)
    , decayMs(0.0f)
    , sustainLevel(1.0f)
    , releaseMs(20.0f)
    , maxTimeMs(1000.0f)  // Default 1 second display range
    , currentAlpha(0.0f)  // Start invisible
{
    setOpaque(false);  // Transparent background
    setVisible(false);  // Start hidden
    setAlpha(0.0f);     // Ensure alpha is 0
}

ADSRVisualizationComponent::~ADSRVisualizationComponent()
{
}

void ADSRVisualizationComponent::setADSR(float attack, float decay, float sustain, float release)
{
    attackMs = attack;
    decayMs = decay;
    sustainLevel = sustain;
    releaseMs = release;
    // Only repaint if we're actually visible (alpha > 0) AND visible
    if (currentAlpha > 0.0f && isVisible()) {
        repaint();
    }
}

float ADSRVisualizationComponent::timeToX(float timeMs, float width) const
{
    // Map time (0 to maxTimeMs) to x coordinate (0 to width)
    return (timeMs / maxTimeMs) * width;
}

float ADSRVisualizationComponent::amplitudeToY(float amplitude, float height) const
{
    // Invert y: amplitude 1.0 = top (y=0), amplitude 0.0 = bottom (y=height)
    return height * (1.0f - amplitude);
}

void ADSRVisualizationComponent::paint(juce::Graphics& g)
{
    // Don't paint if alpha is 0 or component is not visible
    // Use a strict check - if alpha is exactly 0 or less, don't paint at all
    // Check visibility first, then alpha
    if (!isVisible()) {
        return;
    }
    if (currentAlpha <= 0.0f) {
        return;
    }
    
    auto bounds = getLocalBounds().reduced(5.0f); // Add padding
    float width = static_cast<float>(bounds.getWidth());
    float height = static_cast<float>(bounds.getHeight());
    
    if (width <= 0 || height <= 0) {
        return;
    }
    
    // Use local copies to avoid modifying member variables
    float localAttack = attackMs;
    float localDecay = decayMs;
    float localSustain = sustainLevel;
    float localRelease = releaseMs;
    
    // Ensure minimum values for visibility
    if (localAttack < 1.0f) localAttack = 1.0f;
    if (localDecay < 1.0f) localDecay = 1.0f;
    if (localRelease < 1.0f) localRelease = 1.0f;
    
    // Use a fixed visual span for each phase, but map the position based on actual parameter values
    // Attack: 0-10s maps to left-right position, but visual span stays constant
    // Decay: 0-20s maps to position, visual span stays constant
    // Release: 0-20s maps to position, visual span stays constant
    
    // Fixed visual spans (as percentage of width)
    float attackVisualSpan = width * 0.3f;  // Attack phase spans 30% of width
    float decayVisualSpan = width * 0.2f;   // Decay phase spans 20% of width
    float sustainVisualSpan = width * 0.2f; // Sustain phase spans 20% of width
    float releaseVisualSpan = width * 0.3f; // Release phase spans 30% of width
    
    // Map actual parameter values to positions (0-max maps to start of phase)
    // Attack: 0-10000ms maps to 0-30% of width
    float attackMaxMs = 10000.0f;
    float attackPosition = (localAttack / attackMaxMs) * attackVisualSpan;
    float x0 = bounds.getX();
    float x1 = x0 + attackPosition;
    
    // Decay: 0-20000ms maps to position within decay span
    float decayMaxMs = 20000.0f;
    float decayPosition = (localDecay / decayMaxMs) * decayVisualSpan;
    float x2 = x1 + decayPosition;
    
    // Sustain: fixed position
    float x3 = x2 + sustainVisualSpan;
    
    // Release: 0-20000ms maps to position within release span
    float releaseMaxMs = 20000.0f;
    float releasePosition = (localRelease / releaseMaxMs) * releaseVisualSpan;
    float x4 = x3 + releasePosition;
    
    // Ensure we don't exceed bounds
    float rightBound = static_cast<float>(bounds.getRight());
    x1 = juce::jlimit(x0, rightBound, x1);
    x2 = juce::jlimit(x1, rightBound, x2);
    x3 = juce::jlimit(x2, rightBound, x3);
    x4 = juce::jlimit(x3, rightBound, x4);
    
    // Calculate y positions (inverted: top is amplitude 1.0, bottom is 0.0)
    float y0 = bounds.getBottom(); // Start at bottom (amplitude 0)
    float y1 = bounds.getY(); // Attack peak at top (amplitude 1.0)
    float y2 = bounds.getY() + (1.0f - localSustain) * height; // Decay to sustain level
    float y3 = y2; // Sustain holds at same level
    float y4 = bounds.getBottom(); // Release to bottom (amplitude 0)
    
    // Draw smooth envelope path with curves (like the reference)
    juce::Path envelopePath;
    envelopePath.startNewSubPath(x0, y0);
    
    // Attack: curve up to peak (use quadratic curve for smoothness)
    float attackControlX = x0 + (x1 - x0) * 0.5f;
    float attackControlY = y0 - (y0 - y1) * 0.3f; // Control point for smooth curve
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
    
    // Draw the envelope path with light blue color (like the reference)
    juce::Colour lightBlue = juce::Colour(0xFF87CEEB); // Light blue (sky blue)
    lightBlue = lightBlue.withAlpha(currentAlpha); // Apply alpha
    g.setColour(lightBlue);
    g.strokePath(envelopePath, juce::PathStrokeType(2.0f));
    
    // Draw 5 control points (nodes) as small light blue circles
    // 1. Start point (bottom-left)
    g.fillEllipse(x0 - 3.0f, y0 - 3.0f, 6.0f, 6.0f);
    
    // 2. Attack peak
    g.fillEllipse(x1 - 3.0f, y1 - 3.0f, 6.0f, 6.0f);
    
    // 3. End of decay (start of sustain)
    g.fillEllipse(x2 - 3.0f, y2 - 3.0f, 6.0f, 6.0f);
    
    // 4. End of sustain (start of release)
    g.fillEllipse(x3 - 3.0f, y3 - 3.0f, 6.0f, 6.0f);
    
    // 5. End of release (bottom-right)
    g.fillEllipse(x4 - 3.0f, y4 - 3.0f, 6.0f, 6.0f);
}

