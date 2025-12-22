#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace Core {

/**
 * Pop Event - POD only, no strings, lock-free
 */
struct PopEvent {
    uint64_t frameCounterGlobal;  // Monotonic sample counter
    int voiceId;
    int note;
    int envStage;  // 0=Idle, 1=Attack, 2=Decay, 3=Sustain, 4=Release
    double playheadFrame;  // Fractional playhead position
    float envValue;
    float microRampValue;  // rampGain
    float voiceOutL;
    float voiceOutR;
    float voiceDelta;  // max(|voiceOut - lastVoiceOut|)
    float mixOutL;
    float mixOutR;
    float mixDelta;  // max(|mixOut - lastMixOut|)
    uint32_t flags;  // Bitmask: nanGuard(1), oobClamp(2), voiceStolenThisBlock(4), 
                     //          noteOnThisBlock(8), noteOffThisBlock(16)
    
    PopEvent() : frameCounterGlobal(0), voiceId(-1), note(-1), envStage(0),
                 playheadFrame(0.0), envValue(0.0f), microRampValue(0.0f),
                 voiceOutL(0.0f), voiceOutR(0.0f), voiceDelta(0.0f),
                 mixOutL(0.0f), mixOutR(0.0f), mixDelta(0.0f), flags(0) {}
};

/**
 * Lock-free ring buffer for pop events (audio thread writes, UI thread reads)
 * Fixed size, no allocations
 */
class PopEventRingBuffer {
public:
    static constexpr int BUFFER_SIZE = 64;
    
    PopEventRingBuffer() : writeIndex(0), readIndex(0) {
        std::memset(events, 0, sizeof(events));
    }
    
    // Audio thread: try to write (non-blocking, may overwrite old events)
    bool write(const PopEvent& event) {
        int nextWrite = (writeIndex.load(std::memory_order_relaxed) + 1) % BUFFER_SIZE;
        if (nextWrite == readIndex.load(std::memory_order_acquire)) {
            // Buffer full, overwrite oldest
            readIndex.store((readIndex.load(std::memory_order_relaxed) + 1) % BUFFER_SIZE,
                          std::memory_order_release);
        }
        events[writeIndex.load(std::memory_order_relaxed)] = event;
        writeIndex.store(nextWrite, std::memory_order_release);
        return true;
    }
    
    // UI thread: read all available events
    int read(PopEvent* out, int maxCount) {
        int count = 0;
        int currentRead = readIndex.load(std::memory_order_acquire);
        int currentWrite = writeIndex.load(std::memory_order_acquire);
        
        while (count < maxCount && currentRead != currentWrite) {
            out[count] = events[currentRead];
            currentRead = (currentRead + 1) % BUFFER_SIZE;
            count++;
        }
        
        readIndex.store(currentRead, std::memory_order_release);
        return count;
    }
    
    void clear() {
        writeIndex.store(0, std::memory_order_release);
        readIndex.store(0, std::memory_order_release);
    }
    
private:
    PopEvent events[BUFFER_SIZE];
    std::atomic<int> writeIndex;
    std::atomic<int> readIndex;
};

/**
 * Slew Limiter (Click Suppressor) - per-sample rate limiter
 * Hardware-friendly, no allocations
 */
class SlewLimiter {
public:
    SlewLimiter() : lastOutL(0.0f), lastOutR(0.0f), maxStep(0.02f) {}
    
    void setMaxStep(float step) { maxStep = step; }
    
    void process(float& sampleL, float& sampleR) {
        // Clamp to max step per sample
        float targetL = sampleL;
        float targetR = sampleR;
        
        float deltaL = targetL - lastOutL;
        float deltaR = targetR - lastOutR;
        
        if (std::abs(deltaL) > maxStep) {
            sampleL = lastOutL + (deltaL > 0.0f ? maxStep : -maxStep);
        } else {
            sampleL = targetL;
        }
        
        if (std::abs(deltaR) > maxStep) {
            sampleR = lastOutR + (deltaR > 0.0f ? maxStep : -maxStep);
        } else {
            sampleR = targetR;
        }
        
        lastOutL = sampleL;
        lastOutR = sampleR;
    }
    
    void processMono(float& sample) {
        float target = sample;
        float delta = target - lastOutL;
        
        if (std::abs(delta) > maxStep) {
            sample = lastOutL + (delta > 0.0f ? maxStep : -maxStep);
        } else {
            sample = target;
        }
        
        lastOutL = sample;
        lastOutR = sample;
    }
    
    void reset() {
        lastOutL = 0.0f;
        lastOutR = 0.0f;
    }
    
private:
    float lastOutL;
    float lastOutR;
    float maxStep;
};

/**
 * Per-block pop detector (audio thread)
 */
class PopDetector {
public:
    PopDetector() : frameCounter(0), threshold(0.15f) {
        lastMixOutL = 0.0f;
        lastMixOutR = 0.0f;
    }
    
    void processBlock(float** output, int numChannels, int numSamples,
                     PopEventRingBuffer& eventBuffer) {
        if (numChannels == 0 || output[0] == nullptr || numSamples == 0) {
            return;
        }
        
        float maxMixDeltaL = 0.0f;
        float maxMixDeltaR = 0.0f;
        float maxMixAbsL = 0.0f;
        float maxMixAbsR = 0.0f;
        
        // Track first sample of block (compare with last sample of previous block)
        if (numSamples > 0) {
            float firstL = output[0][0];
            float firstR = (numChannels > 1 && output[1] != nullptr) ? output[1][0] : firstL;
            
            float deltaL = std::abs(firstL - lastMixOutL);
            float deltaR = std::abs(firstR - lastMixOutR);
            
            maxMixDeltaL = std::max(maxMixDeltaL, deltaL);
            maxMixDeltaR = std::max(maxMixDeltaR, deltaR);
            maxMixAbsL = std::max(maxMixAbsL, std::abs(firstL));
            maxMixAbsR = std::max(maxMixAbsR, std::abs(firstR));
        }
        
        // Scan block for discontinuities
        for (int i = 1; i < numSamples; ++i) {
            float deltaL = std::abs(output[0][i] - output[0][i-1]);
            float deltaR = (numChannels > 1 && output[1] != nullptr) 
                          ? std::abs(output[1][i] - output[1][i-1]) : deltaL;
            
            maxMixDeltaL = std::max(maxMixDeltaL, deltaL);
            maxMixDeltaR = std::max(maxMixDeltaR, deltaR);
            maxMixAbsL = std::max(maxMixAbsL, std::abs(output[0][i]));
            maxMixAbsR = std::max(maxMixAbsR, std::abs((numChannels > 1 && output[1] != nullptr) 
                                                 ? output[1][i] : output[0][i]));
        }
        
        // Store last samples for next block
        if (numSamples > 0) {
            lastMixOutL = output[0][numSamples - 1];
            lastMixOutR = (numChannels > 1 && output[1] != nullptr) 
                         ? output[1][numSamples - 1] : lastMixOutL;
        }
        
        // Check for pop at mix level
        float maxMixDelta = std::max(maxMixDeltaL, maxMixDeltaR);
        if (maxMixDelta > threshold) {
            PopEvent event;
            event.frameCounterGlobal = frameCounter;
            event.voiceId = -1;  // Mix-level pop, not voice-specific
            event.note = -1;
            event.envStage = -1;
            event.playheadFrame = 0.0;
            event.envValue = 0.0f;
            event.microRampValue = 0.0f;
            event.voiceOutL = 0.0f;
            event.voiceOutR = 0.0f;
            event.voiceDelta = 0.0f;
            event.mixOutL = std::max(maxMixAbsL, maxMixAbsR);
            event.mixOutR = event.mixOutL;
            event.mixDelta = maxMixDelta;
            event.flags = 0;
            
            eventBuffer.write(event);
        }
        
        frameCounter += static_cast<uint64_t>(numSamples);
    }
    
    void setThreshold(float thresh) { threshold = thresh; }
    
    uint64_t getFrameCounter() const { return frameCounter; }
    
private:
    uint64_t frameCounter;
    float threshold;
    float lastMixOutL;
    float lastMixOutR;
};

} // namespace Core
