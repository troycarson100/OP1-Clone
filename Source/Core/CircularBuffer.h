#pragma once

namespace Core {

/**
 * Fixed-size circular buffer for audio processing
 * No dynamic allocations - all memory pre-allocated
 * Portable C++ - no JUCE dependencies
 */
class CircularBuffer {
public:
    CircularBuffer();
    ~CircularBuffer();
    
    /**
     * Initialize buffer with fixed size
     * Must be called before use
     */
    void prepare(int size);
    
    /**
     * Write samples to buffer (overwrites oldest if full)
     */
    void write(const float* data, int numSamples);
    
    /**
     * Read samples from buffer without advancing read pointer
     * Returns number of samples actually available
     */
    int peek(float* output, int numSamples, int offset = 0) const;
    
    /**
     * Read and remove samples from buffer
     * Returns number of samples actually read
     */
    int read(float* output, int numSamples);
    
    /**
     * Get number of samples available to read
     */
    int getNumAvailable() const;
    
    /**
     * Clear all data in buffer
     */
    void clear();
    
    /**
     * Get maximum size
     */
    int getSize() const { return bufferSize; }

private:
    float* buffer;
    int bufferSize;
    int writePos;
    int readPos;
    int numAvailable;
    
    int wrapIndex(int index) const;
};

} // namespace Core

