#include "ADSRPillComponent.h"

ADSRPillComponent::ADSRPillComponent()
    : renderer(std::make_unique<JuceVisualizationRenderer>())
    , attackMs(800.0f)  // Default: 800ms
    , decayMs(0.0f)
    , sustainLevel(1.0f)
    , releaseMs(1000.0f)  // Default: 1000ms (1s)
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

Core::ADSRData ADSRPillComponent::buildADSRData() const
{
    Core::ADSRData data;
    
    data.attackMs = attackMs;
    data.decayMs = decayMs;
    data.sustainLevel = sustainLevel;
    data.releaseMs = releaseMs;
    
    // Set colors
    data.backgroundColor = Core::Color(255, 255, 255);    // White
    data.borderColor = Core::Color(51, 51, 51);          // Dark gray
    data.envelopeColor = Core::Color(95, 163, 209);      // Blue
    
    return data;
}

void ADSRPillComponent::paint(juce::Graphics& g)
{
    // Set graphics context for renderer
    renderer->setGraphicsContext(&g);
    
    // Build ADSR data from current state
    Core::ADSRData adsrData = buildADSRData();
    
    // Get bounds
    auto bounds = getLocalBounds();
    Core::Rectangle renderBounds(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight());
    
    // Render using abstract interface
    renderer->renderADSR(adsrData, renderBounds);
}
