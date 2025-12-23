#include "MoogLadderFilter.h"
#include <cmath>
#include <algorithm>

namespace Core {

MoogLadderFilter::MoogLadderFilter()
    : sampleRate(44100.0)
    , cutoffHz(1000.0f)
    , resonance(1.0f)
    , g(0.0f)
    , resonanceCoeff(0.0f)
    , isPrepared(false)
{
    std::fill(stage, stage + 4, 0.0f);
    std::fill(delay, delay + 4, 0.0f);
}

MoogLadderFilter::~MoogLadderFilter()
{
}

void MoogLadderFilter::prepare(double rate)
{
    sampleRate = rate;
    isPrepared = (rate > 0.0);
    if (isPrepared) {
        updateCoefficients();
        reset();
    }
}

void MoogLadderFilter::setCutoff(float cutoff)
{
    cutoffHz = std::max(20.0f, std::min(20000.0f, cutoff));
    updateCoefficients();
}

void MoogLadderFilter::setResonance(float res)
{
    resonance = std::max(0.0f, std::min(4.0f, res));
    updateCoefficients();
}

void MoogLadderFilter::updateCoefficients()
{
    // Safety check: ensure sample rate is valid
    if (sampleRate <= 0.0) {
        return;
    }
    
    // Improved Moog ladder filter coefficient calculation
    // Based on Antti Huovilainen's improved model
    const float pi = 3.14159265f;
    
    // Calculate normalized frequency
    float w = 2.0f * pi * cutoffHz / static_cast<float>(sampleRate);
    
    // Improved g calculation for better stability and character
    // This gives better frequency response and smoother resonance
    g = 0.9892f * w - 0.4342f * w * w + 0.1381f * w * w * w - 0.0202f * w * w * w * w;
    g = std::max(0.0f, std::min(1.0f, g));
    
    // Improved resonance calculation with better scaling
    // This provides more musical resonance with better character
    // Resonance is scaled more accurately to match analog behavior
    resonanceCoeff = resonance * 4.0f * (1.0f - 0.15f * g);
    resonanceCoeff = std::max(0.0f, std::min(4.0f, resonanceCoeff));
}

float MoogLadderFilter::tanhApprox(float x) const
{
    // Improved tanh approximation for better saturation character
    // This provides smoother saturation with better harmonic content
    x = std::max(-4.0f, std::min(4.0f, x));
    
    // Pade approximant for tanh - more accurate than polynomial
    float x2 = x * x;
    float x4 = x2 * x2;
    return x * (945.0f + 105.0f * x2 + x4) / (945.0f + 420.0f * x2 + 15.0f * x4);
}

float MoogLadderFilter::process(float input)
{
    // Safety check: ensure filter is prepared
    if (!isPrepared || sampleRate <= 0.0) {
        return input; // Pass through if not prepared
    }
    
    // Improved Moog ladder filter (Antti Huovilainen / Stilson-Smith hybrid)
    // This provides better character and more accurate analog modeling
    
    // Input with resonance feedback (improved feedback path)
    float in = input - resonanceCoeff * delay[3];
    
    // Apply saturation (tanh) for analog-like behavior
    // The saturation adds character and prevents instability
    in = tanhApprox(in * 0.5f) * 2.0f; // Scale for better saturation response
    
    // 4-stage ladder filter with improved processing
    float temp = in;
    for (int i = 0; i < 4; ++i) {
        // One-pole low-pass: y = g * x + (1 - g) * y_prev
        // Improved calculation for better frequency response
        stage[i] = g * temp + (1.0f - g) * stage[i];
        temp = stage[i];
        delay[i] = stage[i];
    }
    
    // Output from last stage with slight compensation for better frequency response
    return delay[3] * 1.2f; // Slight gain compensation for better character
}

void MoogLadderFilter::processBlock(const float* input, float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i) {
        output[i] = process(input[i]);
    }
}

void MoogLadderFilter::reset()
{
    std::fill(stage, stage + 4, 0.0f);
    std::fill(delay, delay + 4, 0.0f);
}

} // namespace Core
