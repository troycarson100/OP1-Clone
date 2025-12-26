#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>

namespace Core {

/**
 * Portable ring buffer for planar float audio (no allocations in process)
 * Supports multi-channel audio with power-of-two capacity for efficient wrapping
 */
class AudioRingBuffer {
public:
    AudioRingBuffer() 
        : storage(nullptr)
        , capacity(0)
        , channels(0)
        , readPos(0)
        , writePos(0)
        , available(0)
    {}
    
    ~AudioRingBuffer() {
        deallocate();
    }
    
    // Allocate storage (call from prepare/constructor, not audio thread)
    void allocate(int numChannels, int maxFrames) {
        deallocate();
        channels = numChannels;
        capacity = nextPowerOfTwo(maxFrames);
        storage = new float[static_cast<size_t>(capacity * channels)];
        std::fill(storage, storage + capacity * channels, 0.0f);
        readPos = 0;
        writePos = 0;
        available = 0;
    }
    
    void deallocate() {
        delete[] storage;
        storage = nullptr;
        capacity = 0;
        channels = 0;
        readPos = 0;
        writePos = 0;
        available = 0;
    }
    
    void reset() {
        readPos = 0;
        writePos = 0;
        available = 0;
        if (storage && capacity > 0 && channels > 0) {
            std::fill(storage, storage + capacity * channels, 0.0f);
        }
    }
    
    // Push frames into ring (planar: in[channels][frames])
    int push(const float* const* in, int frames) {
        if (!storage || capacity == 0 || channels == 0 || frames <= 0) {
            return 0;
        }
        
        int toPush = std::min(frames, capacity - available);
        if (toPush <= 0) {
            return 0; // Ring full
        }
        
        for (int f = 0; f < toPush; ++f) {
            int writeIdx = (writePos + f) & (capacity - 1);
            for (int ch = 0; ch < channels; ++ch) {
                storage[writeIdx * channels + ch] = in[ch][f];
            }
        }
        
        writePos = (writePos + toPush) & (capacity - 1);
        available += toPush;
        
        return toPush;
    }
    
    // Pop frames from ring (planar: out[channels][frames])
    int pop(float* const* out, int frames) {
        if (!storage || capacity == 0 || channels == 0 || frames <= 0) {
            return 0;
        }
        
        int toPop = std::min(frames, available);
        if (toPop <= 0) {
            // Ring empty - zero-fill output
            for (int ch = 0; ch < channels; ++ch) {
                std::fill(out[ch], out[ch] + frames, 0.0f);
            }
            return 0;
        }
        
        for (int f = 0; f < toPop; ++f) {
            int readIdx = (readPos + f) & (capacity - 1);
            for (int ch = 0; ch < channels; ++ch) {
                out[ch][f] = storage[readIdx * channels + ch];
            }
        }
        
        readPos = (readPos + toPop) & (capacity - 1);
        available -= toPop;
        
        // Zero-fill remaining if requested more than available
        if (toPop < frames) {
            for (int ch = 0; ch < channels; ++ch) {
                std::fill(out[ch] + toPop, out[ch] + frames, 0.0f);
            }
        }
        
        return toPop;
    }
    
    // Peek frames without consuming (planar: out[channels][frames])
    int peek(float* const* out, int frames) const {
        if (!storage || capacity == 0 || channels == 0 || frames <= 0) {
            return 0;
        }
        
        int toPeek = std::min(frames, available);
        if (toPeek <= 0) {
            for (int ch = 0; ch < channels; ++ch) {
                std::fill(out[ch], out[ch] + frames, 0.0f);
            }
            return 0;
        }
        
        for (int f = 0; f < toPeek; ++f) {
            int readIdx = (readPos + f) & (capacity - 1);
            for (int ch = 0; ch < channels; ++ch) {
                out[ch][f] = storage[readIdx * channels + ch];
            }
        }
        
        // Zero-fill remaining
        if (toPeek < frames) {
            for (int ch = 0; ch < channels; ++ch) {
                std::fill(out[ch] + toPeek, out[ch] + frames, 0.0f);
            }
        }
        
        return toPeek;
    }
    
    // Discard frames without reading
    void discard(int frames) {
        int toDiscard = std::min(frames, available);
        readPos = (readPos + toDiscard) & (capacity - 1);
        available -= toDiscard;
    }
    
    int availableToRead() const { return available; }
    int availableToWrite() const { return capacity - available; }
    int getCapacity() const { return capacity; }
    int getChannels() const { return channels; }
    
private:
    float* storage;
    int capacity;
    int channels;
    int readPos;
    int writePos;
    int available;
    
    static int nextPowerOfTwo(int n) {
        if (n <= 0) return 1;
        int p = 1;
        while (p < n) p <<= 1;
        return p;
    }
};

} // namespace Core



