#include "WindowFunctions.h"
#include <cmath>

namespace Core {

void WindowFunctions::generateHann(float* output, int N)
{
    const double pi = 3.14159265358979323846;
    
    for (int i = 0; i < N; ++i) {
        // Hann window: 0.5 * (1 - cos(2*pi*i/(N-1)))
        output[i] = 0.5f * static_cast<float>(1.0 - std::cos(2.0 * pi * i / (N - 1)));
    }
}

void WindowFunctions::generateHannNormalized(float* output, int N)
{
    generateHann(output, N);
    
    // Normalize for perfect reconstruction with 50% overlap
    // Sum of squared windows should equal N
    float sumSquared = 0.0f;
    for (int i = 0; i < N; ++i) {
        sumSquared += output[i] * output[i];
    }
    
    if (sumSquared > 0.0f) {
        float normFactor = std::sqrt(static_cast<float>(N) / sumSquared);
        for (int i = 0; i < N; ++i) {
            output[i] *= normFactor;
        }
    }
}

float WindowFunctions::calculateNormalizationFactor(const float* window, int N)
{
    float sumSquared = 0.0f;
    for (int i = 0; i < N; ++i) {
        sumSquared += window[i] * window[i];
    }
    
    if (sumSquared > 0.0f) {
        return std::sqrt(static_cast<float>(N) / sumSquared);
    }
    return 1.0f;
}

} // namespace Core

