#pragma once

#include <cstddef>
#include <algorithm>

namespace Core {

/**
 * Fixed-size ring buffer for streaming audio processing
 * Portable C++ - no JUCE dependencies
 * No allocations in process() - storage provided externally
 */
class RingBufferF {
public:
    RingBufferF();
    
    /**
     * Initialize with external storage
     * storage: pre-allocated buffer (must remain valid)
     * capacity: size of storage buffer
     */
    void init(float* storage, int capacity);
    
    /**
     * Get current size (number of samples available)
     */
    int size() const { return size_; }
    
    /**
     * Get capacity
     */
    int capacity() const { return cap_; }
    
    /**
     * Get free space available
     */
    int freeSpace() const { return cap_ - size_; }
    
    /**
     * Push samples into buffer
     * Returns number of samples actually pushed
     */
    int push(const float* in, int n);
    
    /**
     * Peek at samples without removing them
     * offset: offset from read position
     * Returns number of samples actually available
     */
    int peek(float* out, int n, int offset = 0) const;
    
    /**
     * Pop samples from buffer
     * Returns number of samples actually popped
     */
    int pop(float* out, int n);
    
    /**
     * Discard samples without reading them
     */
    void discard(int n);
    
    /**
     * Reset buffer (clear all data)
     */
    void reset();

private:
    float* buf_;
    int cap_;
    int r_;      // Read position
    int w_;      // Write position
    int size_;   // Current size
};

} // namespace Core

