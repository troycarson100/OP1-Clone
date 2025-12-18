#pragma once

namespace Core {

/**
 * Minimal radix-2 FFT implementation
 * Portable C++ - no JUCE dependencies
 * 
 * Frame size must be power of 2 (e.g., 256, 512, 1024, 2048)
 */
class SimpleFFT {
public:
    SimpleFFT();
    ~SimpleFFT();
    
    /**
     * Prepare FFT for given frame size
     * frameSize must be power of 2
     */
    void prepare(int frameSize);
    
    /**
     * Perform forward FFT (time -> frequency)
     * Input: real samples (frameSize)
     * Output: complex spectrum (frameSize/2 + 1 complex numbers = frameSize + 2 floats)
     * Output format: [real0, imag0, real1, imag1, ..., realN, imagN]
     */
    void forward(const float* input, float* output);
    
    /**
     * Perform inverse FFT (frequency -> time)
     * Input: complex spectrum (frameSize + 2 floats)
     * Output: real samples (frameSize)
     */
    void inverse(const float* input, float* output);
    
    /**
     * Get frame size
     */
    int getFrameSize() const { return frameSize; }

private:
    int frameSize;
    float* twiddleFactors;
    float* bitReverseTable;
    int* bitReverseIndices;
    
    bool isPowerOf2(int n) const;
    int log2(int n) const;
    void computeTwiddleFactors();
    void computeBitReverseTable();
};

} // namespace Core

