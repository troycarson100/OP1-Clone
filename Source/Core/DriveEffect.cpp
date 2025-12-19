#include "DriveEffect.h"
#include <algorithm>
#include <cmath>

namespace Core {

DriveEffect::DriveEffect()
    : drive(1.0f)
{
}

DriveEffect::~DriveEffect()
{
}

void DriveEffect::setDrive(float d)
{
    drive = std::max(0.0f, std::min(2.0f, d));
}

float DriveEffect::tanhApprox(float x) const
{
    // Fast tanh approximation for soft saturation
    // Clamp input to prevent overflow
    x = std::max(-3.0f, std::min(3.0f, x));
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

float DriveEffect::process(float input)
{
    if (drive <= 0.0f) {
        return 0.0f;
    }
    
    if (std::abs(drive - 1.0f) < 0.001f) {
        // No drive, pass through
        return input;
    }
    
    // Apply drive: multiply input by drive amount
    float driven = input * drive;
    
    // Apply soft saturation using tanh
    float saturated = tanhApprox(driven);
    
    // Normalize: divide by drive to maintain level
    // This ensures that at drive=1.0, output ≈ input
    return saturated / drive;
}

void DriveEffect::processBlock(const float* input, float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i) {
        output[i] = process(input[i]);
    }
}

void DriveEffect::reset()
{
    // No state to reset
}

} // namespace Core

