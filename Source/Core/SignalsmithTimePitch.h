#pragma once

#include "ITimePitch.h"
#include "RingBufferF.h"

// Forward declaration - actual Signalsmith Stretch header will be included in .cpp
// This keeps the header JUCE-free and portable
// Note: SignalsmithStretch is a template struct, but we'll use PIMPL to hide it

namespace Core {

/**
 * Wrapper around Signalsmith Stretch library
 * Implements ITimePitch interface for high-quality time-stretching and pitch-shifting
 * 
 * This is a temporary implementation using Signalsmith Stretch.
 * For embedded hardware, replace with a custom implementation of ITimePitch.
 */
class SignalsmithTimePitch : public ITimePitch {
public:
    SignalsmithTimePitch();
    ~SignalsmithTimePitch() override;
    
    void prepare(const TimePitchConfig& config) override;
    void reset() override;
    void setPitchSemitones(float semitones) override;
    void setTimeRatio(float ratio) override;
    int process(const float* in, int inN, float* out, int outN) override;
    int getInputLatency() const override;
    int getOutputLatency() const override;
    int flush(float* out, int outN) override;
    bool isPrepared() const override;

private:
    // PIMPL pattern - hide Signalsmith implementation details
    class Impl;
    Impl* impl;
    
    TimePitchConfig currentConfig;
    float currentPitchSemitones;
    float currentTimeRatio;
    int inputLatency;
    int outputLatency;
    bool prepared;
    
    // Internal ring buffers for handling small host blocks (96 samples)
    // Accumulates input and output to avoid starving the stretcher
    RingBufferF inputRing;
    RingBufferF outputRing;
    float* inputRingStorage;
    float* outputRingStorage;
    int inputRingCapacity;
    int outputRingCapacity;
    
    // Temporary buffer for stretcher processing (larger chunks)
    float* tempInputBuffer;
    float* tempOutputBuffer;
    int tempBufferSize;
    
    // Priming state: after reset, need to accumulate input before producing output
    bool needsPriming;
    
    void allocateBuffers();
    void deallocateBuffers();
    
    // Helper: push samples into input ring
    void pushToInputRing(const float* in, int n);
    
    // Helper: pull samples from output ring
    int pullFromOutputRing(float* out, int n);
    
    // Helper: process accumulated input through stretcher
    void processAccumulatedInput();
};

} // namespace Core

