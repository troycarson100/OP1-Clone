#pragma once

#include <cstddef>

namespace Core {

class GranularTimeWarp {
public:
    GranularTimeWarp();
    ~GranularTimeWarp();

    void prepare(double sampleRate, int maxBlockSize);
    void setPitchRatio(float ratio);
    void setTimeRatio(float ratio);
    void reset();

    int process(const float* in, int inCount, float* out, int outCapacity);

private:
    static constexpr int FRAME_SIZE = 256;
    static constexpr int HOP_SIZE = 128; // 50% overlap

    double sampleRate = 44100.0;
    float pitchRatio = 1.0f;
    float timeRatio = 1.0f;

    float* window = nullptr;
    float* grainBuffer = nullptr;
    float* olaBuffer = nullptr;

    double readPos = 0.0;

    void allocateBuffers(int maxBlockSize);
    void deallocateBuffers();
};

} // namespace Core
