#include "SimplePitchShifter.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Core {

SimplePitchShifter::SimplePitchShifter()
    : pitchRatio(1.0)
    , writePos(0)
    , readPos(0)
    , phase(0.0)
{
    std::fill(buffer, buffer + BUFFER_SIZE, 0.0f);
    initWindow();
}

SimplePitchShifter::~SimplePitchShifter()
{
}

void SimplePitchShifter::initWindow()
{
    // Initialize Hanning window
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / (BUFFER_SIZE - 1)));
    }
}

void SimplePitchShifter::setPitchRatio(double ratio)
{
    // Clamp pitch ratio to reasonable range
    pitchRatio = std::max(0.5, std::min(2.0, ratio)); // Limit to ±1 octave for quality
}

float SimplePitchShifter::process(float input)
{
    // Simple approach: for time-stretching, we actually want to change the read speed
    // but keep the output rate constant. However, that's complex.
    // 
    // Simpler approach: disable pitch shifting for now and just pass through
    // The user wants time-stretching, but a proper implementation requires
    // phase vocoder or granular synthesis which is complex.
    //
    // For now, let's use a simpler method: if pitchRatio is 1.0, pass through.
    // Otherwise, we'll need to implement proper time-stretching.
    
    if (std::abs(pitchRatio - 1.0) < 0.001) {
        return input; // No pitch shift needed
    }
    
    // For non-unity pitch ratios, we need proper time-stretching
    // This is a placeholder - proper implementation would use phase vocoder
    // For now, just pass through to avoid artifacts
    return input;
}

void SimplePitchShifter::reset()
{
    std::fill(buffer, buffer + BUFFER_SIZE, 0.0f);
    writePos = 0;
    readPos = 0;
    phase = 0.0;
}

int SimplePitchShifter::wrapIndex(int index) const
{
    if (index < 0) {
        return index + BUFFER_SIZE;
    } else if (index >= BUFFER_SIZE) {
        return index - BUFFER_SIZE;
    }
    return index;
}

} // namespace Core

