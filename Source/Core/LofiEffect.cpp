#include "LofiEffect.h"
#include <algorithm>
#include <cmath>

namespace Core {

LofiEffect::LofiEffect()
    : sampleRate(44100.0)
    , bitDepth(16.0f)
    , sampleRateReduction(1.0f)
    , holdSample(0.0f)
    , holdCounter(0.0f)
    , holdInterval(1.0f)
{
}

LofiEffect::~LofiEffect()
{
}

void LofiEffect::prepare(double rate)
{
    sampleRate = rate;
    updateHoldInterval();
    reset();
}

void LofiEffect::setBitDepth(float bits)
{
    bitDepth = std::max(1.0f, std::min(16.0f, bits));
}

void LofiEffect::setSampleRateReduction(float factor)
{
    sampleRateReduction = std::max(0.01f, std::min(1.0f, factor));
    updateHoldInterval();
}

float LofiEffect::calculateQuantizationStep() const
{
    if (bitDepth <= 0.0f) return 1.0f;
    
    // Calculate number of quantization levels
    // For bit depth N, we have 2^N levels
    int levels = static_cast<int>(std::pow(2.0f, bitDepth));
    if (levels < 2) levels = 2;
    
    // Step size is 2.0 / (levels - 1) to cover range [-1, 1]
    return 2.0f / static_cast<float>(levels - 1);
}

float LofiEffect::quantize(float sample) const
{
    // Clamp input to [-1, 1]
    sample = std::max(-1.0f, std::min(1.0f, sample));
    
    float step = calculateQuantizationStep();
    
    // Quantize to nearest step
    float quantized = std::round(sample / step) * step;
    
    return quantized;
}

void LofiEffect::updateHoldInterval()
{
    if (sampleRate <= 0.0 || sampleRateReduction <= 0.0f) {
        holdInterval = 1.0f;
        return;
    }
    
    // Calculate how many samples to hold before updating
    // Lower reduction factor = hold longer (more lofi)
    // At 1.0 (no reduction), holdInterval = 1 (update every sample)
    // At 0.1 (10% rate), holdInterval = 10 (update every 10 samples)
    holdInterval = 1.0f / sampleRateReduction;
    
    // Clamp to reasonable range
    holdInterval = std::max(1.0f, std::min(100.0f, holdInterval));
}

float LofiEffect::process(float input)
{
    // Sample rate reduction: hold sample for multiple samples
    holdCounter += 1.0f;
    
    if (holdCounter >= holdInterval) {
        // Update held sample
        holdSample = input;
        holdCounter = 0.0f;
    }
    
    // Apply bitcrushing to held sample
    float output = quantize(holdSample);
    
    return output;
}

void LofiEffect::processBlock(const float* input, float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i) {
        output[i] = process(input[i]);
    }
}

void LofiEffect::reset()
{
    holdSample = 0.0f;
    holdCounter = 0.0f;
}

} // namespace Core



