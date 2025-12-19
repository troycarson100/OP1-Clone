#include "SignalsmithTimePitch.h"

// Include Signalsmith Stretch library
// NOTE: Library must be vendored in ThirdParty/signalsmith/
// Download from: https://github.com/Signalsmith-Audio/signalsmith-stretch
// Also requires Signalsmith Linear in ThirdParty/signalsmith-linear/
#include "../../ThirdParty/signalsmith/signalsmith-stretch.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <vector>
#include <fstream>
#include <chrono>

namespace Core {

// PIMPL implementation to hide Signalsmith details
class SignalsmithTimePitch::Impl {
public:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    
    Impl() = default;
    ~Impl() = default;
};

SignalsmithTimePitch::SignalsmithTimePitch()
    : impl(nullptr)
    , currentConfig{1, 44100.0, 512}
    , currentPitchSemitones(0.0f)
    , currentTimeRatio(1.0f)
    , inputLatency(0)
    , outputLatency(0)
    , prepared(false)
    , inputRingStorage(nullptr)
    , outputRingStorage(nullptr)
    , inputRingCapacity(0)
    , outputRingCapacity(0)
    , tempInputBuffer(nullptr)
    , tempOutputBuffer(nullptr)
    , tempBufferSize(0)
    , needsPriming(false)
{
    impl = new Impl();
}

SignalsmithTimePitch::~SignalsmithTimePitch()
{
    deallocateBuffers();
    delete impl;
}

void SignalsmithTimePitch::prepare(const TimePitchConfig& config)
{
    currentConfig = config;
    
    // Configure Signalsmith Stretch for very low latency
    // Using manual configure with much smaller block/interval sizes
    // presetCheaper: blockSamples = sampleRate*0.1, intervalSamples = sampleRate*0.04
    // Very low latency config: blockSamples = sampleRate*0.04, intervalSamples = sampleRate*0.01
    // This significantly reduces latency but uses more CPU
    // splitComputation=true spreads CPU load across blocks to prevent spikes
    double blockSamplesConfig = config.sampleRate * 0.04;  // Further reduced for very low latency
    double intervalSamplesConfig = config.sampleRate * 0.01;  // Further reduced for very low latency
    // Ensure minimum sizes for stability (at least 64 samples)
    int configuredBlockSamples = std::max(64, static_cast<int>(blockSamplesConfig));
    int configuredIntervalSamples = std::max(32, static_cast<int>(intervalSamplesConfig));
    impl->stretch.configure(config.channels, configuredBlockSamples, configuredIntervalSamples, true);
    
    // Get latency values and block sizes
    inputLatency = impl->stretch.inputLatency();
    outputLatency = impl->stretch.outputLatency();
    int blockSamples = impl->stretch.blockSamples();
    int intervalSamples = impl->stretch.intervalSamples();
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"SignalsmithTimePitch.cpp:65\",\"message\":\"Stretcher prepared\",\"data\":{\"sampleRate\":" << config.sampleRate << ",\"channels\":" << config.channels << ",\"inputLatency\":" << inputLatency << ",\"outputLatency\":" << outputLatency << ",\"blockSamples\":" << blockSamples << ",\"intervalSamples\":" << intervalSamples << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // Allocate internal buffers for handling small blocks
    allocateBuffers();
    
    // Reset state and mark as prepared
    reset();
    prepared = true;
    needsPriming = false; // Will be set to true by reset()
}

void SignalsmithTimePitch::reset()
{
    if (!prepared || impl == nullptr) {
        return;
    }
    
    // Actually reset the stretcher
    impl->stretch.reset();
    
    // Re-apply current pitch transpose after reset
    impl->stretch.setTransposeSemitones(std::max(-24.0f, std::min(24.0f, currentPitchSemitones)));
    
    // Clear ring buffers
    inputRing.reset();
    outputRing.reset();
    
    // CRITICAL: After reset, the stretcher needs to be primed with input
    // According to the library docs, after reset() the processing time is inputLatency()
    // samples BEFORE the first input. We should use seek() to provide initial input,
    // but for streaming we'll accumulate input naturally. Mark that we need priming.
    needsPriming = true;
}

void SignalsmithTimePitch::setPitchSemitones(float semitones)
{
    currentPitchSemitones = semitones;
    
    // Clamp to reasonable range
    semitones = std::max(-24.0f, std::min(24.0f, semitones));
    
    // Set transpose in semitones
    impl->stretch.setTransposeSemitones(semitones);
}

void SignalsmithTimePitch::setTimeRatio(float ratio)
{
    currentTimeRatio = ratio;
    
    // Clamp to reasonable range
    currentTimeRatio = std::max(0.25f, std::min(4.0f, ratio));
    
    // For constant duration (pitch-only), timeRatio should be 1.0
    // Signalsmith Stretch infers time-stretch from in/out buffer sizes
    // With timeRatio=1.0, we use inN=outN (1:1 ratio)
}

int SignalsmithTimePitch::process(const float* in, int inN, float* out, int outN)
{
    if (!prepared || impl == nullptr) {
        return 0;
    }
    
    if (out == nullptr || outN <= 0) {
        return 0;
    }
    
    int inputRingSizeBefore = inputRing.size();
    int outputRingSizeBefore = outputRing.size();
    
    // Push input into input ring buffer
    if (in != nullptr && inN > 0) {
        pushToInputRing(in, inN);
    }
    
    // Process accumulated input through stretcher until we have enough output
    // This handles small host blocks (96 samples) by accumulating
    // For polyphony: limit iterations to prevent CPU spikes when multiple voices process
    const int maxIterations = 3; // Reduced from 20 to prevent CPU spikes with chords
    int iterations = 0;
    
    while (outputRing.size() < outN && iterations < maxIterations) {
        // Process if we have enough input available (wait for reasonable chunks)
        // This prevents CPU spikes when multiple voices are active
        if (inputRing.size() >= 64) { // Wait for at least 64 samples to reduce processing frequency
            processAccumulatedInput();
        } else {
            // Not enough input - break (will accumulate more on next call)
            break;
        }
        iterations++;
    }
    
    // Pull output from ring buffer
    // CRITICAL FIX: Only pull what we need, don't drain the ring completely
    // This allows output to accumulate across calls for polyphony
    // Pull up to outN, but don't pull more than what's available
    int actualOutN = pullFromOutputRing(out, outN);
    
    // If we got less than requested, that's OK - we'll accumulate more on next call
    // But we should NOT return 0 if we have ANY output available
    // The issue is that we're pulling all available output, leaving nothing for next call
    
    // Count non-zero samples for debugging
    int nonZeroCount = 0;
    for (int i = 0; i < actualOutN; ++i) {
        if (std::abs(out[i]) > 1e-6f) {
            nonZeroCount++;
        }
    }
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"B\",\"location\":\"SignalsmithTimePitch.cpp:109\",\"message\":\"Stretcher process\",\"data\":{\"inN\":" << inN << ",\"outN\":" << outN << ",\"actualOutN\":" << actualOutN << ",\"inputRingBefore\":" << inputRingSizeBefore << ",\"inputRingAfter\":" << inputRing.size() << ",\"outputRingBefore\":" << outputRingSizeBefore << ",\"outputRingAfter\":" << outputRing.size() << ",\"iterations\":" << iterations << ",\"nonZeroCount\":" << nonZeroCount << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // Return actual output count
    // If we got less than requested, that's OK - we'll get more on next call
    // The voice will mix whatever we produce
    return actualOutN;
}

int SignalsmithTimePitch::getInputLatency() const
{
    return inputLatency;
}

int SignalsmithTimePitch::getOutputLatency() const
{
    return outputLatency;
}

bool SignalsmithTimePitch::isPrepared() const
{
    return prepared;
}

int SignalsmithTimePitch::flush(float* out, int outN)
{
    if (out == nullptr || outN <= 0) {
        return 0;
    }
    
    // Feed silence to flush remaining samples
    // Feed zeros equal to input latency
    int silenceSamples = inputLatency;
    if (silenceSamples > 0) {
        std::vector<float> silence(static_cast<size_t>(silenceSamples), 0.0f);
        pushToInputRing(silence.data(), silenceSamples);
        
        // Process to drain
        processAccumulatedInput();
    }
    
    // Pull any remaining output
    return pullFromOutputRing(out, outN);
}

void SignalsmithTimePitch::pushToInputRing(const float* in, int n)
{
    if (in == nullptr || n <= 0 || inputRingStorage == nullptr) {
        return;
    }
    
    // Push samples into the ring buffer (RingBufferF handles the copying)
    int pushed = inputRing.push(in, n);
    
    // Sanity check: if we couldn't push all samples, the ring is full
    // This shouldn't happen in normal operation, but handle gracefully
    if (pushed < n) {
        // Ring buffer full - this is a problem, but continue
    }
}

int SignalsmithTimePitch::pullFromOutputRing(float* out, int n)
{
    if (out == nullptr || n <= 0 || outputRingStorage == nullptr) {
        return 0;
    }
    
    // CRITICAL FIX: Pull only what's available, don't drain completely
    // This ensures output accumulates across calls for polyphony
    // Each voice needs its own output stream
    int available = outputRing.size();
    int toPull = std::min(n, available);
    
    if (toPull > 0) {
        // Pull what we can
        int pulled = outputRing.pop(out, toPull);
        
        // Zero-fill remaining if needed
        if (pulled < n) {
            std::fill(out + pulled, out + n, 0.0f);
        }
        
        return pulled;
    }
    
    // No output available - return 0 (will be zero-filled by caller)
    std::fill(out, out + n, 0.0f);
    return 0;
}

void SignalsmithTimePitch::processAccumulatedInput()
{
    if (!prepared || impl == nullptr || tempInputBuffer == nullptr || tempOutputBuffer == nullptr) {
        return;
    }
    
    // CRITICAL FIX: Signalsmith Stretch uses STFT internally and needs to accumulate input
    // The stretcher processes output sample-by-sample, but only processes new STFT blocks
    // when it has accumulated enough input (at least intervalSamples).
    // We need to accumulate input until we have enough for the stretcher to process blocks.
    int intervalSamples = impl->stretch.intervalSamples();
    int blockSamples = impl->stretch.blockSamples();
    
    // For polyphony: process larger chunks less frequently to reduce CPU load
    // The stretcher accumulates input internally, so we can process in larger batches
    const int minChunkSize = 64; // Increased from 32 to reduce processing frequency
    const int preferredChunkSize = std::max(128, intervalSamples / 2); // Process in larger chunks, but not full interval
    int inputAvailable = inputRing.size();
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"E\",\"location\":\"SignalsmithTimePitch.cpp:260\",\"message\":\"processAccumulatedInput called\",\"data\":{\"inputAvailable\":" << inputAvailable << ",\"minChunkSize\":" << minChunkSize << ",\"preferredChunkSize\":" << preferredChunkSize << ",\"intervalSamples\":" << intervalSamples << ",\"blockSamples\":" << blockSamples << ",\"outputRingSize\":" << outputRing.size() << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    if (inputAvailable < minChunkSize) {
        // Not enough input yet - wait for more
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"B\",\"location\":\"SignalsmithTimePitch.cpp:275\",\"message\":\"Not enough input, returning early\",\"data\":{\"inputAvailable\":" << inputAvailable << ",\"minChunkSize\":" << minChunkSize << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
        return;
    }
    
    // Process what we have, up to preferred chunk size
    // CRITICAL: Process larger chunks to give the stretcher enough input to work with
    int processSize = std::min(inputAvailable, preferredChunkSize);
    
    // Ensure temp buffers are large enough
    if (processSize > tempBufferSize) {
        // This shouldn't happen, but handle gracefully
        processSize = tempBufferSize;
    }
    
    int peeked = inputRing.peek(tempInputBuffer, processSize);
    if (peeked >= minChunkSize) {
        // For constant duration (timeRatio=1.0), use 1:1 input/output
        const float* inputBuffers[1] = {tempInputBuffer};
        float* outputBuffers[1] = {tempOutputBuffer};
        
        // CRITICAL FIX: The stretcher processes output sample-by-sample.
        // With 1:1 ratio (timeRatio=1.0), it calculates: inputOffset = round(outputIndex * inputSamples / outputSamples)
        // Since inputSamples == outputSamples, this becomes inputOffset = outputIndex.
        // So for outputIndex N, it needs input up to position N.
        // We're providing peeked input and requesting peeked output, which should work.
        // The stretcher will produce output sample-by-sample as it processes, even if some are zeros initially.
        int outputRequested = peeked; // Request same amount of output as input (1:1 ratio)
        impl->stretch.process(inputBuffers, peeked, outputBuffers, outputRequested);
        
        // Mark that we've started processing (no longer need priming)
        if (needsPriming) {
            needsPriming = false;
        }
        
        // Mark that we've started processing (no longer need priming)
        if (needsPriming) {
            needsPriming = false;
        }
        
        // Sanitize output samples
        int nonZeroInOutput = 0;
        for (int i = 0; i < outputRequested; ++i) {
            if (!std::isfinite(tempOutputBuffer[i])) {
                tempOutputBuffer[i] = 0.0f;
            } else if (std::abs(tempOutputBuffer[i]) > 1e-6f) {
                nonZeroInOutput++;
            }
        }
        
        // Push output to output ring (use actual output produced)
        outputRing.push(tempOutputBuffer, outputRequested);
        
        // Discard consumed input
        inputRing.discard(peeked);
        
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"E\",\"location\":\"SignalsmithTimePitch.cpp:315\",\"message\":\"Processed chunk through stretcher\",\"data\":{\"peeked\":" << peeked << ",\"outputRingSizeAfter\":" << outputRing.size() << ",\"nonZeroInOutput\":" << nonZeroInOutput << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
    }
}

void SignalsmithTimePitch::allocateBuffers()
{
    deallocateBuffers();
    
    // Allocate ring buffers (enough for several blocks + latency)
    inputRingCapacity = currentConfig.maxBlockSize * 4 + inputLatency;
    outputRingCapacity = currentConfig.maxBlockSize * 4 + outputLatency;
    
    inputRingStorage = new float[static_cast<size_t>(inputRingCapacity)];
    outputRingStorage = new float[static_cast<size_t>(outputRingCapacity)];
    
    inputRing.init(inputRingStorage, inputRingCapacity);
    outputRing.init(outputRingStorage, outputRingCapacity);
    
    // Allocate temp buffers for processing chunks
    // CRITICAL: Make temp buffers large enough for at least one interval
    // The stretcher needs larger chunks to process STFT blocks effectively
    int intervalSamples = impl->stretch.intervalSamples();
    int blockSamples = impl->stretch.blockSamples();
    tempBufferSize = std::max(256, intervalSamples + blockSamples); // At least interval+block size
    tempInputBuffer = new float[static_cast<size_t>(tempBufferSize)];
    tempOutputBuffer = new float[static_cast<size_t>(tempBufferSize)];
    
    std::fill(inputRingStorage, inputRingStorage + inputRingCapacity, 0.0f);
    std::fill(outputRingStorage, outputRingStorage + outputRingCapacity, 0.0f);
    std::fill(tempInputBuffer, tempInputBuffer + tempBufferSize, 0.0f);
    std::fill(tempOutputBuffer, tempOutputBuffer + tempBufferSize, 0.0f);
}

void SignalsmithTimePitch::deallocateBuffers()
{
    delete[] inputRingStorage;
    inputRingStorage = nullptr;
    
    delete[] outputRingStorage;
    outputRingStorage = nullptr;
    
    delete[] tempInputBuffer;
    tempInputBuffer = nullptr;
    
    delete[] tempOutputBuffer;
    tempOutputBuffer = nullptr;
    
    inputRingCapacity = 0;
    outputRingCapacity = 0;
    tempBufferSize = 0;
}

} // namespace Core

