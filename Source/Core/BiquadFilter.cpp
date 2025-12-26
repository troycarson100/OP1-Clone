#include "BiquadFilter.h"
#include <cmath>
#include <algorithm>

namespace Core {

BiquadFilter::BiquadFilter()
    : sampleRate(44100.0)
    , cutoffHz(1000.0f)
    , resonance(1.0f)
    , a0(1.0f), a1(0.0f), a2(0.0f)
    , b1(0.0f), b2(0.0f)
    , x1(0.0f), x2(0.0f)
    , y1(0.0f), y2(0.0f)
{
}

BiquadFilter::~BiquadFilter()
{
}

void BiquadFilter::prepare(double rate)
{
    sampleRate = rate;
    updateCoefficients();
    reset();
}

void BiquadFilter::setCutoff(float cutoff)
{
    cutoffHz = std::max(20.0f, std::min(20000.0f, cutoff));
    updateCoefficients();
}

void BiquadFilter::setResonance(float res)
{
    resonance = std::max(0.1f, std::min(10.0f, res));
    updateCoefficients();
}

void BiquadFilter::updateCoefficients()
{
    // Standard biquad low-pass filter design
    const float w0 = 2.0f * 3.14159265f * cutoffHz / static_cast<float>(sampleRate);
    const float cosw0 = std::cos(w0);
    const float sinw0 = std::sin(w0);
    const float alpha = sinw0 / (2.0f * resonance);
    
    const float b0 = (1.0f - cosw0) / 2.0f;
    const float b1 = 1.0f - cosw0;
    const float b2 = (1.0f - cosw0) / 2.0f;
    const float a0_inv = 1.0f / (1.0f + alpha);
    
    // Normalize coefficients
    this->a0 = b0 * a0_inv;
    this->a1 = b1 * a0_inv;
    this->a2 = b2 * a0_inv;
    this->b1 = -2.0f * cosw0 * a0_inv;
    this->b2 = (1.0f - alpha) * a0_inv;
}

float BiquadFilter::process(float input)
{
    // Direct Form I biquad implementation
    float output = a0 * input + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
    
    // Update state
    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;
    
    return output;
}

void BiquadFilter::processBlock(const float* input, float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i) {
        output[i] = process(input[i]);
    }
}

void BiquadFilter::reset()
{
    x1 = x2 = 0.0f;
    y1 = y2 = 0.0f;
}

} // namespace Core




