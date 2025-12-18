#pragma once

#include "STFT.h"
#include "CircularBuffer.h"

namespace Core {

/**
 * Pitch Shifter with Time-Scale Modification (TSM)
 * Implements Phase Vocoder algorithm based on Bernsee's approach
 * 
 * Algorithm overview:
 * - Uses STFT with constant hop size (Ha = Hs) to maintain constant duration
 * - Pitch shifting achieved by scaling phase advances in frequency domain
 * - Phase unwrapping and propagation for smooth pitch changes
 * 
 * Frame/hop choices:
 * - Frame size: 1024 or 2048 (larger = better quality, more CPU, more latency)
 * - Hop: frameSize/4 (75% overlap, standard for phase vocoder)
 * - Why constant duration: Ha = Hs means time-scale = 1.0, duration unchanged
 * 
 * Quality vs CPU tradeoffs:
 * - Larger frame size: better frequency resolution, smoother pitch shifts, more CPU
 * - Smaller hop: better time resolution, more CPU (more frames to process)
 * - Phase locking: optional improvement for transients (not implemented yet)
 */
class PitchShiftTSM {
public:
    PitchShiftTSM();
    ~PitchShiftTSM();
    
    /**
     * Prepare pitch shifter
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
     * Process audio block
     * Input and output buffers must be pre-allocated
     * numSamples: number of samples to process
     * Returns number of output samples generated (may be less than numSamples due to latency)
     */
    int process(const float* input, float* output, int numSamples);
    
    /**
     * Reset internal state (clear buffers, reset phases)
     */
    void reset();
    
    /**
     * Get latency in samples (due to frame size and overlap)
     */
    int getLatency() const { return frameSize - hopSize; }

private:
    // STFT parameters
    static constexpr int DEFAULT_FRAME_SIZE = 1024;
    static constexpr int DEFAULT_HOP_SIZE = 256; // frameSize / 4
    
    int frameSize;
    int hopSize;
    double sampleRate;
    
    STFT stft;
    CircularBuffer inputBuffer;
    CircularBuffer outputBuffer;
    
    // Phase vocoder state
    float* lastPhase;
    float* currentPhase;
    float* magnitude;
    float* complexSpectrum;
    
    float pitchRatio;
    int numBins;
    
    // Simple resampling state (temporary until FFT is fixed)
    double readHead;
    
    // Transient detection (simple spectral flux)
    float* lastMagnitude;
    float spectralFluxThreshold;
    
    void allocateBuffers();
    void deallocateBuffers();
    void processFrame();
    void unwrapPhase(float* phase, const float* lastPhase, int numBins);
    float detectTransient(const float* magnitude);
};

} // namespace Core

