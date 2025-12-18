#pragma once

#include "SimpleFFT.h"
#include "WindowFunctions.h"

namespace Core {

/**
 * Short-Time Fourier Transform (STFT) for analysis and synthesis
 * Portable C++ - no JUCE dependencies
 * 
 * Frame size and hop size choices:
 * - Frame size N: 1024 or 2048 (power of 2) - larger = better frequency resolution, more latency
 * - Hop size H: N/4 (75% overlap) - standard for phase vocoder, good quality
 * - Why constant duration: Ha = Hs (analysis hop = synthesis hop) means time-scale = 1.0
 *   Pitch is changed by manipulating phases in frequency domain, not by changing hop ratio
 */
class STFT {
public:
    STFT();
    ~STFT();
    
    /**
     * Prepare STFT with frame size and hop size
     * frameSize must be power of 2
     * hopSize typically frameSize/4 for 75% overlap
     */
    void prepare(int frameSize, int hopSize, double sampleRate);
    
    /**
     * Analyze one frame (time -> frequency)
     * Input: frameSize samples from input buffer
     * Output: complex spectrum (frameSize + 2 floats)
     */
    void analyze(const float* input, float* output);
    
    /**
     * Synthesize one frame (frequency -> time)
     * Input: complex spectrum (frameSize + 2 floats)
     * Output: frameSize samples (add to output buffer with overlap-add)
     */
    void synthesize(const float* input, float* output);
    
    /**
     * Get frame size
     */
    int getFrameSize() const { return frameSize; }
    
    /**
     * Get hop size
     */
    int getHopSize() const { return hopSize; }

private:
    int frameSize;
    int hopSize;
    double sampleRate;
    
    SimpleFFT fft;
    float* window;
    float* analysisBuffer;
    float* synthesisBuffer;
    float* complexSpectrum;
    
    void allocateBuffers();
    void deallocateBuffers();
};

} // namespace Core

