#pragma once

#include "VisualizationData.h"

namespace Core {

/**
 * Abstract visualization renderer interface
 * Pure C++ - no JUCE dependencies
 * 
 * Implementations can use JUCE Graphics (in JuceWrapper) or custom hardware rendering
 */
class IVisualizationRenderer {
public:
    virtual ~IVisualizationRenderer() = default;
    
    /**
     * Render waveform visualization
     * @param data Waveform data to render
     * @param bounds Rectangle bounds for rendering
     */
    virtual void renderWaveform(const WaveformData& data, const Rectangle& bounds) = 0;
    
    /**
     * Render ADSR envelope visualization
     * @param data ADSR data to render
     * @param bounds Rectangle bounds for rendering (pill shape)
     */
    virtual void renderADSR(const ADSRData& data, const Rectangle& bounds) = 0;
    
    /**
     * Clear/fill background
     * @param color Background color
     * @param bounds Rectangle bounds to fill
     */
    virtual void fillBackground(const Color& color, const Rectangle& bounds) = 0;
};

} // namespace Core

