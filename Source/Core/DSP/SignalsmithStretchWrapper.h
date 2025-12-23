#pragma once

#include "IWarpProcessor.h"
#include "AudioRingBuffer.h"
#include "../../ThirdParty/signalsmith/signalsmith-stretch.h"
#include <memory>
#include <atomic>

namespace Core {

/**
 * Wrapper for Signalsmith Stretch with proper buffering and latency handling
 * Portable C++ - no JUCE dependencies
 */
class SignalsmithStretchWrapper : public IWarpProcessor {
public:
    SignalsmithStretchWrapper();
    ~SignalsmithStretchWrapper() override;
    
    // IWarpProcessor interface
    void prepare(double sampleRate, int channels, int maxBlockFrames) override;
    void reset() override;
    void setTimeRatio(double ratio) override;
    void setPitchSemitones(float semitones) override;
    int process(const float* const* in, int inFrames,
                float* const* out, int outFrames) override;
    int getLatencyFrames() const override;
    bool isPrepared() const override;
    int flush(float* const* out, int outFrames) override;
    int getOutputRingFill() const override;
    
    // Debug metrics (atomic, lock-free)
    float getWarpPeak() const { return warpPeak.load(std::memory_order_acquire); }
    float getWarpMaxDelta() const { return warpMaxDelta.load(std::memory_order_acquire); }
    int getInputStarveCount() const { return inputStarveCount.load(std::memory_order_acquire); }
    int getOutputUnderrunCount() const { return outputUnderrunCount.load(std::memory_order_acquire); }
    float getGainMatch() const { return gainMatch.load(std::memory_order_acquire); }
    float getLimiterGain() const { return limiterGain.load(std::memory_order_acquire); }

private:
    // PIMPL to hide Signalsmith implementation
    class Impl;
    std::unique_ptr<Impl> impl;
    
    // State
    bool prepared_;
    double sampleRate_;
    int channels_;
    int maxBlockFrames_;
    double timeRatio_;
    float pitchSemitones_;
    int inputLatency_;
    int outputLatency_;
    bool primed_;
    
    // Ring buffers
    AudioRingBuffer inputRing_;
    AudioRingBuffer outputRing_;
    
    // Temporary buffers for processing chunks
    float** tempIn_;
    float** tempOut_;
    int tempInMaxFrames_;
    int tempOutMaxFrames_;
    int chunkOutFrames_;  // Fixed internal processing chunk size
    
    // Debug metrics (atomic, lock-free)
    mutable std::atomic<float> warpPeak{0.0f};
    mutable std::atomic<float> warpMaxDelta{0.0f};
    mutable std::atomic<int> inputStarveCount{0};
    mutable std::atomic<int> outputUnderrunCount{0};
    mutable std::atomic<float> gainMatch{1.0f};
    mutable std::atomic<float> limiterGain{1.0f};
    
    void allocateBuffers();
    void deallocateBuffers();
};

} // namespace Core

