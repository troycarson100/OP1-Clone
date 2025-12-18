#include "Resampler.h"
#include "TimePitchError.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>  // For std::abs
#include <cfloat>   // For FLT_EPSILON, isfinite

namespace Core {

Resampler::Resampler()
    : sampleRate(44100.0)
    , ratio(1.0f)
    , inputBufferStorage(nullptr)
    , inputBufferCapacity(0)
    , inputWritePos(0)
    , inputReadStart(0)
    , inputSize(0)
    , readPos(0.0)
    , lowpassState(0.0f)
    , lowpassAlpha(0.0f)
{
}

Resampler::~Resampler()
{
    deallocateBuffers();
}

void Resampler::prepare(double rate)
{
    sampleRate = rate;
    allocateBuffers();
    reset();
}

void Resampler::setRatio(float r)
{
    // Clamp to reasonable range and check for invalid values
    if (!std::isfinite(r) || r <= 0.001f) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::BAD_RATIO);
        ratio = 1.0f;
        updateLowpass();
        return;
    }
    ratio = std::max(0.25f, std::min(4.0f, r));
    updateLowpass();
}

void Resampler::reset()
{
    if (inputBufferStorage != nullptr && inputBufferCapacity > 0) {
        std::fill(inputBufferStorage, inputBufferStorage + inputBufferCapacity, 0.0f);
    }
    inputWritePos = 0;
    inputReadStart = 0;
    inputSize = 0;
    readPos = 0.0;
    lowpassState = 0.0f;
}

int Resampler::process(const float* in, int inCount, float* out, int outCapacity)
{
    // Defensive checks
    if (in == nullptr || out == nullptr) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::NULL_BUFFER);
        std::fill(out, out + outCapacity, 0.0f);
        return 0;
    }
    
    if (inCount <= 0 || outCapacity <= 0) {
        return 0;
    }
    
    // Validate ratio
    if (!std::isfinite(ratio) || ratio <= 0.001f) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::BAD_RATIO);
        // Bypass and pass through
        int toCopy = std::min(inCount, outCapacity);
        std::memcpy(out, in, toCopy * sizeof(float));
        return toCopy;
    }
    
    // No resampling needed - pass through
    if (std::abs(ratio - 1.0f) < 0.001f) {
        int toCopy = std::min(inCount, outCapacity);
        std::memcpy(out, in, toCopy * sizeof(float));
        return toCopy;
    }
    
    // Simple linear interpolation resampling with bounds checking
    // This avoids the complex ring buffer that was causing crashes
    int outputSamples = 0;
    double localReadPos = 0.0;  // Local read position for this block
    
    for (int i = 0; i < outCapacity && outputSamples < outCapacity; ++i) {
        int idx0 = static_cast<int>(localReadPos);
        int idx1 = idx0 + 1;
        
        // Clamp indices to valid range (never read negative or beyond)
        idx0 = std::max(0, std::min(idx0, inCount - 1));
        idx1 = std::max(0, std::min(idx1, inCount - 1));
        
        float sample = 0.0f;
        
        if (localReadPos >= static_cast<double>(inCount)) {
            // Past end of input - output zero
            sample = 0.0f;
        } else if (idx0 == idx1 || idx1 >= inCount) {
            // Last sample or same sample
            sample = in[idx0];
        } else {
            // Linear interpolation
            float frac = static_cast<float>(localReadPos - static_cast<double>(idx0));
            frac = std::max(0.0f, std::min(1.0f, frac));  // Clamp
            sample = in[idx0] * (1.0f - frac) + in[idx1] * frac;
        }
        
        // Check for NaN/Inf
        if (!std::isfinite(sample)) {
            sample = 0.0f;
        }
        
        out[outputSamples] = sample;
        outputSamples++;
        localReadPos += ratio;
        
        if (localReadPos >= static_cast<double>(inCount)) {
            // Past end - zero pad remaining
            break;
        }
    }
    
    return outputSamples;
    
    /* ORIGINAL RING BUFFER CODE - DISABLED DUE TO CRASHES
    // Push input into ring buffer with lowpass filtering if needed
    int inputPushed = 0;
    for (int i = 0; i < inCount && inputSize < inputBufferCapacity - 1; ++i) {
        float sample = in[i];
        
        // Apply lowpass if ratio > 1 (pitch up - need anti-aliasing)
        if (ratio > 1.0f) {
            lowpassState = lowpassState + lowpassAlpha * (sample - lowpassState);
            sample = lowpassState;
        }
        
        if (inputWritePos >= 0 && inputWritePos < inputBufferCapacity) {
            inputBufferStorage[inputWritePos] = sample;
            inputWritePos = (inputWritePos + 1) % inputBufferCapacity;
            inputSize++;
            inputPushed++;
        } else {
            break; // Safety check failed
        }
    }
    
    // Resample from input buffer
    // Input buffer is a ring buffer: samples are at positions [0, inputSize)
    // readPos tracks fractional position in this range (relative to inputReadStart)
    int outputSamples = 0;
    
    while (outputSamples < outCapacity && readPos < static_cast<double>(inputSize - 3) && inputSize >= 4) {
        // Get 4 samples for cubic interpolation
        int idx0 = static_cast<int>(readPos);
        int idx1 = idx0 + 1;
        int idx2 = idx0 + 2;
        int idx3 = idx0 + 3;
        
        // Safety check
        if (idx3 >= inputSize) {
            break;
        }
        
        // Map to ring buffer positions
        int pos0 = (inputReadStart + idx0) % inputBufferCapacity;
        int pos1 = (inputReadStart + idx1) % inputBufferCapacity;
        int pos2 = (inputReadStart + idx2) % inputBufferCapacity;
        int pos3 = (inputReadStart + idx3) % inputBufferCapacity;
        
        // Safety check for buffer capacity
        if (pos0 >= inputBufferCapacity || pos1 >= inputBufferCapacity || 
            pos2 >= inputBufferCapacity || pos3 >= inputBufferCapacity) {
            break;
        }
        
        float y0 = inputBufferStorage[pos0];
        float y1 = inputBufferStorage[pos1];
        float y2 = inputBufferStorage[pos2];
        float y3 = inputBufferStorage[pos3];
        
        float frac = static_cast<float>(readPos - static_cast<double>(idx0));
        out[outputSamples] = cubicInterpolate(y0, y1, y2, y3, frac);
        
        outputSamples++;
        readPos += ratio;
    }
    
    // Discard consumed input
    int consumed = static_cast<int>(readPos);
    if (consumed > 0 && consumed <= inputSize) {
        inputReadStart = (inputReadStart + consumed) % inputBufferCapacity;
        inputSize -= consumed;
        readPos -= static_cast<double>(consumed);
        
        // Reset if buffer is mostly empty
        if (inputSize < 4) {
            readPos = 0.0;
            inputReadStart = 0;
        }
    }
    
    return outputSamples;
    */
}

float Resampler::cubicInterpolate(float y0, float y1, float y2, float y3, float frac)
{
    // Catmull-Rom spline interpolation
    float frac2 = frac * frac;
    float frac3 = frac2 * frac;
    
    float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c = -0.5f * y0 + 0.5f * y2;
    float d = y1;
    
    return a * frac3 + b * frac2 + c * frac + d;
}

void Resampler::updateLowpass()
{
    // Simple 1-pole lowpass for anti-aliasing
    // Cutoff ~ 0.45/r of Nyquist when ratio > 1
    if (ratio > 1.0f) {
        float nyquist = static_cast<float>(sampleRate * 0.5);
        float cutoff = 0.45f * nyquist / ratio;
        float rc = 1.0f / (2.0f * 3.14159265359f * cutoff);
        float dt = 1.0f / static_cast<float>(sampleRate);
        lowpassAlpha = dt / (rc + dt);
    } else {
        lowpassAlpha = 0.0f;  // No filtering needed
    }
}

void Resampler::allocateBuffers()
{
    deallocateBuffers();
    
    // Input buffer needs enough space for interpolation (4 samples) + some extra
    inputBufferCapacity = 4096;  // Large enough for streaming
    inputBufferStorage = new float[inputBufferCapacity];
    std::fill(inputBufferStorage, inputBufferStorage + inputBufferCapacity, 0.0f);
    
    updateLowpass();
}

void Resampler::deallocateBuffers()
{
    delete[] inputBufferStorage;
    inputBufferStorage = nullptr;
}

} // namespace Core

