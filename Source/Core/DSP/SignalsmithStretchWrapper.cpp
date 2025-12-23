#include "SignalsmithStretchWrapper.h"
#include "AudioRingBuffer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <climits>

namespace Core {

// PIMPL implementation
class SignalsmithStretchWrapper::Impl {
public:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    
    Impl() = default;
    ~Impl() = default;
};

SignalsmithStretchWrapper::SignalsmithStretchWrapper()
    : impl(std::make_unique<Impl>())
    , prepared_(false)
    , sampleRate_(44100.0)
    , channels_(2)
    , maxBlockFrames_(512)
    , timeRatio_(1.0)
    , pitchSemitones_(0.0f)
    , inputLatency_(0)
    , outputLatency_(0)
    , primed_(false)
    , tempIn_(nullptr)
    , tempOut_(nullptr)
    , tempInMaxFrames_(0)
    , tempOutMaxFrames_(0)
    , chunkOutFrames_(256)
{
}

SignalsmithStretchWrapper::~SignalsmithStretchWrapper() {
    deallocateBuffers();
}

void SignalsmithStretchWrapper::prepare(double sampleRate, int channels, int maxBlockFrames) {
    sampleRate_ = sampleRate;
    channels_ = channels;
    maxBlockFrames_ = maxBlockFrames;
    
    // Configure Signalsmith Stretch for highest quality (like Ableton Complex)
    // Use larger blocks and smaller intervals for better quality (more CPU but cleaner)
    // Block size: 0.15 * sampleRate (larger = better quality, more latency)
    // Interval: 0.02 * sampleRate (smaller = better time resolution)
    float blockSize = static_cast<float>(sampleRate) * 0.15f;  // Larger blocks for better quality
    float intervalSize = static_cast<float>(sampleRate) * 0.02f;  // Smaller intervals for better resolution
    impl->stretch.configure(channels, static_cast<int>(blockSize), static_cast<int>(intervalSize), false);
    
    // Get latency values
    inputLatency_ = impl->stretch.inputLatency();
    outputLatency_ = impl->stretch.outputLatency();
    
    // Determine chunk size (fixed internal processing chunk)
    // Use larger chunks for better quality (but more latency)
    chunkOutFrames_ = std::max(512, std::min(1024, maxBlockFrames));
    
    // Allocate buffers
    allocateBuffers();
    
    // Reset state
    reset();
    
    prepared_ = true;
}

void SignalsmithStretchWrapper::reset() {
    if (!prepared_) {
        return;
    }
    
    // Reset the stretcher
    impl->stretch.reset();
    
    // Re-apply current settings
    impl->stretch.setTransposeSemitones(std::max(-24.0f, std::min(24.0f, pitchSemitones_)));
    
    // Clear ring buffers
    inputRing_.reset();
    outputRing_.reset();
    
    // Reset priming state
    primed_ = false;
    
    // Reset debug metrics
    warpPeak.store(0.0f, std::memory_order_relaxed);
    warpMaxDelta.store(0.0f, std::memory_order_relaxed);
    inputStarveCount.store(0, std::memory_order_relaxed);
    outputUnderrunCount.store(0, std::memory_order_relaxed);
    gainMatch.store(1.0f, std::memory_order_relaxed);
    limiterGain.store(1.0f, std::memory_order_relaxed);
}

void SignalsmithStretchWrapper::setTimeRatio(double ratio) {
    timeRatio_ = std::max(0.25, std::min(4.0, ratio));
    // Time ratio is handled by input/output sample counts in process()
}

void SignalsmithStretchWrapper::setPitchSemitones(float semitones) {
    pitchSemitones_ = std::max(-24.0f, std::min(24.0f, semitones));
    if (prepared_) {
        impl->stretch.setTransposeSemitones(pitchSemitones_);
    }
}

int SignalsmithStretchWrapper::process(const float* const* in, int inFrames,
                                       float* const* out, int outFrames) {
    if (!prepared_ || out == nullptr || outFrames <= 0) {
        return 0;
    }
    
    // Reset debug metrics for this block
    warpPeak.store(0.0f, std::memory_order_relaxed);
    warpMaxDelta.store(0.0f, std::memory_order_relaxed);
    
    // (1) Push input frames into inputRing
    if (in != nullptr && inFrames > 0) {
        inputRing_.push(in, inFrames);
    }
    
    // (2) Process chunks until outputRing has enough frames
    // Signalsmith infers time ratio from inputSamples/outputSamples ratio
    const int maxIterations = 8; // Prevent runaway loops
    int iterations = 0;
    
    while (outputRing_.availableToRead() < outFrames && iterations < maxIterations) {
        iterations++;
        
        // Determine output chunk size
        int outChunk = chunkOutFrames_;
        
        // Determine input chunk size based on time ratio
        // For timeRatio = 1.0: inChunk = outChunk (1:1)
        // For timeRatio = 0.5: inChunk = outChunk / 2 (slow down 2x)
        // For timeRatio = 2.0: inChunk = outChunk * 2 (speed up 2x)
        int inChunk = static_cast<int>(std::round(static_cast<double>(outChunk) / timeRatio_));
        inChunk = std::max(32, std::min(inChunk, tempInMaxFrames_));
        
        // Pop input chunk from inputRing
        int actualInChunk = inputRing_.pop(tempIn_, inChunk);
        
        // Track starvation: if we got less than requested
        if (actualInChunk < inChunk) {
            // Pad remaining with zeros
            for (int ch = 0; ch < channels_; ++ch) {
                std::fill(tempIn_[ch] + actualInChunk, tempIn_[ch] + inChunk, 0.0f);
            }
            if (actualInChunk == 0) {
                inputStarveCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        
        // Clear output buffer
        for (int ch = 0; ch < channels_; ++ch) {
            std::fill(tempOut_[ch], tempOut_[ch] + outChunk, 0.0f);
        }
        
        // Prepare planar pointers for Signalsmith
        const float* inPtrs[2] = {tempIn_[0], (channels_ > 1) ? tempIn_[1] : tempIn_[0]};
        float* outPtrs[2] = {tempOut_[0], (channels_ > 1) ? tempOut_[1] : tempOut_[0]};
        
        // Process with Signalsmith Stretch
        // Time ratio is inferred from inChunk/outChunk ratio
        impl->stretch.process(inPtrs, inChunk, outPtrs, outChunk);
        
        // Check for non-finite values
        for (int ch = 0; ch < channels_; ++ch) {
            for (int f = 0; f < outChunk; ++f) {
                if (!std::isfinite(tempOut_[ch][f])) {
                    tempOut_[ch][f] = 0.0f;
                }
            }
        }
        
        // Push to output ring
        outputRing_.push(tempOut_, outChunk);
        
        // If we got no input and no more output available, break
        if (actualInChunk == 0) {
            break;
        }
    }
    
    // Track underruns
    int finalFill = outputRing_.availableToRead();
    if (finalFill < outFrames) {
        outputUnderrunCount.fetch_add(1, std::memory_order_relaxed);
    }
    
    // (3) Pop exactly outFrames from outputRing
    int popped = outputRing_.pop(out, outFrames);
    
    // Track discontinuities and peak for debug metrics
    if (popped > 0) {
        float blockMaxDelta = 0.0f;
        float blockPeak = 0.0f;
        float lastSample[2] = {0.0f, 0.0f};
        
        for (int ch = 0; ch < channels_; ++ch) {
            for (int f = 0; f < popped; ++f) {
                float sample = out[ch][f];
                float absSample = std::abs(sample);
                if (absSample > blockPeak) {
                    blockPeak = absSample;
                }
                
                if (f > 0) {
                    float delta = std::abs(sample - lastSample[ch]);
                    if (delta > blockMaxDelta) {
                        blockMaxDelta = delta;
                    }
                }
                lastSample[ch] = sample;
            }
        }
        
        warpMaxDelta.store(blockMaxDelta, std::memory_order_relaxed);
        warpPeak.store(blockPeak, std::memory_order_relaxed);
    }
    
    // Update priming state
    if (!primed_ && outputRing_.availableToRead() >= outputLatency_) {
        primed_ = true;
    }
    
    return popped;
}

int SignalsmithStretchWrapper::flush(float* const* out, int outFrames) {
    if (!prepared_ || out == nullptr || outFrames <= 0) {
        return 0;
    }
    
    // Process with zero input until output ring drains
    int totalFlushed = 0;
    const int maxFlushIterations = 16;
    int iterations = 0;
    
    // Create zero input
    for (int ch = 0; ch < channels_; ++ch) {
        std::fill(tempIn_[ch], tempIn_[ch] + tempInMaxFrames_, 0.0f);
    }
    
    // Create mutable pointers for advancing
    float* outPtrs[2] = {const_cast<float*>(out[0]), (channels_ > 1) ? const_cast<float*>(out[1]) : const_cast<float*>(out[0])};
    
    // Process with zero input to drain remaining output
    while (totalFlushed < outFrames && iterations < maxFlushIterations) {
        iterations++;
        
        // Process a small chunk with zero input
        int flushChunk = std::min(256, outFrames - totalFlushed);
        flushChunk = std::min(flushChunk, tempOutMaxFrames_);
        
        const float* inPtrs[2] = {tempIn_[0], (channels_ > 1) ? tempIn_[1] : tempIn_[0]};
        float* tempOutPtrs[2] = {tempOut_[0], (channels_ > 1) ? tempOut_[1] : tempOut_[0]};
        
        // Process with zero input
        impl->stretch.process(inPtrs, 0, tempOutPtrs, flushChunk);
        
        // Check for non-finite values
        for (int ch = 0; ch < channels_; ++ch) {
            for (int f = 0; f < flushChunk; ++f) {
                if (!std::isfinite(tempOut_[ch][f])) {
                    tempOut_[ch][f] = 0.0f;
                }
            }
        }
        
        // Push to output ring
        outputRing_.push(tempOut_, flushChunk);
        
        // Pop from output ring
        int toFlush = std::min(flushChunk, outFrames - totalFlushed);
        int flushed = outputRing_.pop(outPtrs, toFlush);
        
        if (flushed > 0) {
            // Advance output pointers
            for (int ch = 0; ch < channels_; ++ch) {
                outPtrs[ch] += flushed;
            }
            totalFlushed += flushed;
        } else {
            // No more output available
            break;
        }
    }
    
    return totalFlushed;
}

int SignalsmithStretchWrapper::getLatencyFrames() const {
    return outputLatency_;
}

bool SignalsmithStretchWrapper::isPrepared() const {
    return prepared_;
}

int SignalsmithStretchWrapper::getOutputRingFill() const {
    return outputRing_.availableToRead();
}

void SignalsmithStretchWrapper::allocateBuffers() {
    deallocateBuffers();
    
    // Allocate ring buffers (enough for 4x max block size)
    int ringCapacity = maxBlockFrames_ * 4;
    inputRing_.allocate(channels_, ringCapacity);
    outputRing_.allocate(channels_, ringCapacity);
    
    // Allocate temporary buffers for processing chunks
    tempInMaxFrames_ = chunkOutFrames_ * 4; // Enough for time ratio up to 4x
    tempOutMaxFrames_ = chunkOutFrames_;
    
    tempIn_ = new float*[static_cast<size_t>(channels_)];
    tempOut_ = new float*[static_cast<size_t>(channels_)];
    
    for (int ch = 0; ch < channels_; ++ch) {
        tempIn_[ch] = new float[static_cast<size_t>(tempInMaxFrames_)];
        tempOut_[ch] = new float[static_cast<size_t>(tempOutMaxFrames_)];
        std::fill(tempIn_[ch], tempIn_[ch] + tempInMaxFrames_, 0.0f);
        std::fill(tempOut_[ch], tempOut_[ch] + tempOutMaxFrames_, 0.0f);
    }
}

void SignalsmithStretchWrapper::deallocateBuffers() {
    if (tempIn_ != nullptr) {
        for (int ch = 0; ch < channels_; ++ch) {
            delete[] tempIn_[ch];
        }
        delete[] tempIn_;
        tempIn_ = nullptr;
    }
    
    if (tempOut_ != nullptr) {
        for (int ch = 0; ch < channels_; ++ch) {
            delete[] tempOut_[ch];
        }
        delete[] tempOut_;
        tempOut_ = nullptr;
    }
    
    tempInMaxFrames_ = 0;
    tempOutMaxFrames_ = 0;
}

} // namespace Core

