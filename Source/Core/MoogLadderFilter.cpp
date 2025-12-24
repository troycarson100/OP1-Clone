#include "MoogLadderFilter.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace Core {

// Internal implementation - pure C++ Moog ladder filter
// Based on Huovilainen method (4-pole, 24dB/octave)
struct MoogLadderFilterImpl {
    double sampleRate;
    float cutoffHz;
    float resonance;  // 0.0-4.0
    float drive;      // Drive amount (>= 1.0)
    
    // State variables (4 cascaded stages)
    float stage1;
    float stage2;
    float stage3;
    float stage4;
    
    // Filter coefficients
    float g;  // Cutoff coefficient
    float resonanceCoeff;  // Resonance feedback coefficient
    
    MoogLadderFilterImpl()
        : sampleRate(44100.0)
        , cutoffHz(20000.0f)
        , resonance(0.0f)
        , drive(1.0f)
        , stage1(0.0f)
        , stage2(0.0f)
        , stage3(0.0f)
        , stage4(0.0f)
        , g(0.0f)
        , resonanceCoeff(0.0f)
    {
    }
    
    void updateCoefficients() {
        if (sampleRate <= 0.0) {
            return;
        }
        
        // Calculate cutoff coefficient g
        // g = tan(pi * fc / fs) for bilinear transform
        const float fc = std::max(20.0f, std::min(20000.0f, cutoffHz));
        const float w = 2.0f * 3.14159265f * fc / static_cast<float>(sampleRate);
        g = static_cast<float>(0.5 * w);  // Simplified approximation for stability
        
        // Clamp g to prevent instability
        g = std::max(0.0f, std::min(1.0f, g));
        
        // Calculate resonance coefficient
        // Map resonance 0-4 to feedback amount
        // At resonance=4, we want strong feedback (can self-oscillate)
        const float clampedRes = std::max(0.0f, std::min(4.0f, resonance));
        resonanceCoeff = clampedRes * 0.25f;  // 0-4 maps to 0-1 feedback
    }
    
    // Fast tanh approximation for drive saturation
    float tanhApprox(float x) const {
        // Pade approximation: tanh(x) ≈ x * (27 + x^2) / (27 + 9*x^2)
        // Clamped for stability
        const float x2 = x * x;
        const float num = x * (27.0f + x2);
        const float den = 27.0f + 9.0f * x2;
        return num / den;
    }
    
    // Apply drive saturation
    float applyDrive(float input) const {
        if (drive <= 1.0f) {
            return input;  // No drive
        }
        // Apply drive: input is multiplied by drive, then tanh-saturated
        return tanhApprox(input * drive) / drive;
    }
};

MoogLadderFilter::MoogLadderFilter()
    : pimpl(std::make_unique<MoogLadderFilterImpl>())
{
}

MoogLadderFilter::~MoogLadderFilter()
{
    // pimpl will be automatically destroyed
}

void MoogLadderFilter::prepare(double rate)
{
    pimpl->sampleRate = rate;
    pimpl->updateCoefficients();
    reset();
    
    // Set initial parameters
    if (pimpl->sampleRate > 0.0) {
        setCutoff(20000.0f);  // Start fully open
        setResonance(0.0f);
        setDrive(1.0f);  // Clean, no drive initially
    }
}

void MoogLadderFilter::setCutoff(float cutoff)
{
    const float clampedCutoff = std::max(20.0f, std::min(20000.0f, cutoff));
    pimpl->cutoffHz = clampedCutoff;
    pimpl->updateCoefficients();
}

void MoogLadderFilter::setCutoffNoUpdate(float cutoff)
{
    // For now, we always update coefficients immediately
    // Could add smoothing here if needed
    setCutoff(cutoff);
}

void MoogLadderFilter::setResonance(float res)
{
    const float clampedRes = std::max(0.0f, std::min(4.0f, res));
    pimpl->resonance = clampedRes;
    pimpl->updateCoefficients();
}

void MoogLadderFilter::setDrive(float drv)
{
    // Drive mapping: 0.0-1.0+ maps to 1.0-10.0+
    // 0.0 = no drive (1.0), 1.0 = strong drive (10.0)
    const float clampedDrive = std::max(0.0f, drv);
    float mappedDrive;
    if (clampedDrive <= 1.0f) {
        // Map 0.0-1.0 to 1.0-10.0
        mappedDrive = 1.0f + (clampedDrive * 9.0f);  // 0.0->1.0, 1.0->10.0
    } else {
        // For values > 1.0, scale even more aggressively
        mappedDrive = 10.0f + ((clampedDrive - 1.0f) * 5.0f);
    }
    pimpl->drive = mappedDrive;
}

float MoogLadderFilter::process(float input)
{
    // Apply drive saturation
    float x = pimpl->applyDrive(input);
    
    // Calculate feedback from resonance
    float feedback = pimpl->resonanceCoeff * (pimpl->stage4 - pimpl->stage3);
    
    // Input with feedback
    float u = x - feedback;
    
    // Clamp to prevent instability
    u = std::max(-4.0f, std::min(4.0f, u));
    
    // 4 cascaded one-pole low-pass stages
    // Each stage: y = g*u + (1-g)*y_prev
    const float g = pimpl->g;
    const float oneMinusG = 1.0f - g;
    
    pimpl->stage1 = g * u + oneMinusG * pimpl->stage1;
    pimpl->stage2 = g * pimpl->stage1 + oneMinusG * pimpl->stage2;
    pimpl->stage3 = g * pimpl->stage2 + oneMinusG * pimpl->stage3;
    pimpl->stage4 = g * pimpl->stage3 + oneMinusG * pimpl->stage4;
    
    // Denormal protection (flush to zero if very small)
    if (std::abs(pimpl->stage1) < 1e-10f) pimpl->stage1 = 0.0f;
    if (std::abs(pimpl->stage2) < 1e-10f) pimpl->stage2 = 0.0f;
    if (std::abs(pimpl->stage3) < 1e-10f) pimpl->stage3 = 0.0f;
    if (std::abs(pimpl->stage4) < 1e-10f) pimpl->stage4 = 0.0f;
    
    return pimpl->stage4;
}

void MoogLadderFilter::processBlock(const float* input, float* output, int numSamples)
{
    if (input == nullptr || output == nullptr || numSamples <= 0) {
        return;
    }
    
    for (int i = 0; i < numSamples; ++i) {
        output[i] = process(input[i]);
    }
}

void MoogLadderFilter::reset()
{
    pimpl->stage1 = 0.0f;
    pimpl->stage2 = 0.0f;
    pimpl->stage3 = 0.0f;
    pimpl->stage4 = 0.0f;
}

} // namespace Core
