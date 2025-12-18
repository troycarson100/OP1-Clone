#pragma once

#include <cmath>

namespace Core {

/**
 * Window function utilities
 * Portable C++ - no JUCE dependencies
 */
inline void makeHann(float* w, int N) {
    const float twoPi = 6.283185307179586f;
    if (N <= 1) {
        if (N == 1) w[0] = 1.0f;
        return;
    }
    for (int i = 0; i < N; ++i) {
        w[i] = 0.5f * (1.0f - std::cos(twoPi * static_cast<float>(i) / static_cast<float>(N - 1)));
    }
}

} // namespace Core

