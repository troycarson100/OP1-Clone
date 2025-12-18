#include "GranularTimeWarp.h"
#include "Window.h"
#include "TimePitchError.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Core {

GranularTimeWarp::GranularTimeWarp() = default;

GranularTimeWarp::~GranularTimeWarp() {
    deallocateBuffers();
}

void GranularTimeWarp::prepare(double rate, int maxBlockSize) {
    sampleRate = rate;
    allocateBuffers(maxBlockSize);
    reset();
}

void GranularTimeWarp::setPitchRatio(float ratio) {
    if (!std::isfinite(ratio) || ratio <= 0.001f) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::BAD_RATIO);
        pitchRatio = 1.0f;
        return;
    }
    pitchRatio = std::max(0.25f, std::min(4.0f, ratio));
}

void GranularTimeWarp::setTimeRatio(float ratio) {
    if (!std::isfinite(ratio) || ratio <= 0.001f) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::BAD_RATIO);
        timeRatio = 1.0f;
        return;
    }
    timeRatio = std::max(0.25f, std::min(4.0f, ratio));
}

void GranularTimeWarp::reset() {
    readPos = 0.0;
    if (grainBuffer) std::fill(grainBuffer, grainBuffer + FRAME_SIZE, 0.0f);
    if (olaBuffer) std::fill(olaBuffer, olaBuffer + FRAME_SIZE + HOP_SIZE * 2, 0.0f);
}

int GranularTimeWarp::process(const float* in, int inCount, float* out, int outCapacity) {
    if (!in || !out || inCount <= 0 || outCapacity <= 0) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::NULL_BUFFER);
        if (out) std::fill(out, out + outCapacity, 0.0f);
        return 0;
    }

    int produced = 0;
    std::fill(out, out + outCapacity, 0.0f);

    // Simple per-block granular OLA: generate as many HOP_SIZE chunks as fit
    while (produced + HOP_SIZE <= outCapacity) {
        // Check enough input for a frame
        // We read FRAME_SIZE samples at constant speed (1:1), but with pitch shift applied
        // Max position is readPos + FRAME_SIZE / pitchRatio (for pitch down, we need more input)
        double maxReadPos = readPos + static_cast<double>(FRAME_SIZE) / static_cast<double>(std::max(0.25f, pitchRatio));
        if (maxReadPos >= static_cast<double>(inCount)) {
            TimePitchErrorStatus::getInstance().setError(TimePitchError::WSOLA_UNDERFLOW);
            break;
        }

        // Gather grain with linear interpolation
        // For constant duration: read input at constant speed (1:1), apply pitch shift by resampling
        // Read grains from input at constant spacing, but apply pitch shift by reading at different rates
        for (int i = 0; i < FRAME_SIZE; ++i) {
            // Read from input at constant speed (1:1) to maintain duration
            // Apply pitch shift by reading at positions spaced by 1/pitchRatio
            // This effectively stretches/compresses the grain to change pitch while maintaining duration
            double pos = readPos + static_cast<double>(i) / static_cast<double>(pitchRatio);
            int idx0 = static_cast<int>(pos);
            int idx1 = idx0 + 1;
            if (idx0 < 0) idx0 = 0;
            if (idx0 >= inCount) idx0 = inCount - 1;
            if (idx1 < 0) idx1 = 0;
            if (idx1 >= inCount) idx1 = inCount - 1;
            float frac = static_cast<float>(pos - static_cast<double>(idx0));
            frac = std::max(0.0f, std::min(1.0f, frac));
            float s0 = in[idx0];
            float s1 = in[idx1];
            float g = s0 * (1.0f - frac) + s1 * frac;
            grainBuffer[i] = g * window[i];
        }

        // Overlap-add into out at current produced offset
        for (int i = 0; i < FRAME_SIZE; ++i) {
            int outIndex = produced + i;
            if (outIndex >= outCapacity) break;
            float sample = grainBuffer[i];
            if (!std::isfinite(sample)) sample = 0.0f;
            out[outIndex] += sample;
        }

        // Output always advances by HOP_SIZE (constant duration)
        produced += HOP_SIZE;
        // Input read position advances by HOP_SIZE (constant speed, maintains duration)
        readPos += static_cast<double>(HOP_SIZE);
    }

    return produced;
}

void GranularTimeWarp::allocateBuffers(int /*maxBlockSize*/) {
    deallocateBuffers();
    window = new float[FRAME_SIZE];
    grainBuffer = new float[FRAME_SIZE];
    olaBuffer = new float[FRAME_SIZE + HOP_SIZE * 2];
    makeHann(window, FRAME_SIZE);
    reset();
}

void GranularTimeWarp::deallocateBuffers() {
    delete[] window;
    delete[] grainBuffer;
    delete[] olaBuffer;
    window = nullptr;
    grainBuffer = nullptr;
    olaBuffer = nullptr;
}

} // namespace Core
