#pragma once

#include "GranularTimeWarp.h"

namespace Core {

// Thin wrapper around GranularTimeWarp
// Portable C++ - no JUCE dependencies
class TimePitchProcessor {
public:
    TimePitchProcessor();
    ~TimePitchProcessor();
    
    /**
     * Prepare processor
     * sampleRate: audio sample rate
     * maxBlockSize: maximum block size for processing
     */
    void prepare(double sampleRate, int maxBlockSize);
    
    /**
     * Set pitch ratio
     * ratio = 2^(semitones/12)
     * 1.0 = original pitch, 2.0 = octave up, 0.5 = octave down
     */
    void setPitchRatio(float ratio);
    
    /**
     * Enable/disable processing
     * When disabled, passes through input unchanged
     */
    void setEnabled(bool enabled);
    
    /**
     * Reset internal state
     */
    void reset();
    
    /**
     * Process audio
     * in: input samples
     * inSamples: number of input samples
     * out: output buffer
     * outSamplesWanted: number of output samples desired
     * Returns: number of output samples actually produced
     */
    int process(const float* in, int inSamples, float* out, int outSamplesWanted);

private:
    bool enabled;
    float pitchRatio;
    double sampleRate;
    int maxBlockSize;
    
    GranularTimeWarp granular;
};

} // namespace Core

