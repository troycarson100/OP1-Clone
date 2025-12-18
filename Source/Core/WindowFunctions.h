#pragma once

namespace Core {

/**
 * Window functions for STFT analysis/synthesis
 * Portable C++ - no JUCE dependencies
 */
class WindowFunctions {
public:
    /**
     * Generate a Hann window of size N
     * Output buffer must be pre-allocated with size >= N
     */
    static void generateHann(float* output, int N);
    
    /**
     * Generate a Hann window with normalization
     * Normalized so sum of squared window = N (for perfect reconstruction)
     */
    static void generateHannNormalized(float* output, int N);
    
    /**
     * Calculate window normalization factor
     * Returns factor to multiply window by for perfect reconstruction
     */
    static float calculateNormalizationFactor(const float* window, int N);
};

} // namespace Core

