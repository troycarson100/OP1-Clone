#include "TimePitchProcessor.h"
#include "TimePitchError.h"
#include <algorithm>
#include <cstring>
#include <cfloat>  // For isfinite

namespace Core {

TimePitchProcessor::TimePitchProcessor()
    : enabled(true)
    , pitchRatio(1.0f)
    , sampleRate(44100.0)
    , maxBlockSize(512)
{
}

TimePitchProcessor::~TimePitchProcessor() = default;

void TimePitchProcessor::prepare(double rate, int maxBlock)
{
    sampleRate = rate;
    maxBlockSize = maxBlock;
    
    granular.prepare(rate, maxBlock);
    reset();
}

void TimePitchProcessor::setPitchRatio(float ratio)
{
    // Defensive check
    if (!std::isfinite(ratio) || ratio <= 0.001f) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::BAD_RATIO);
        pitchRatio = 1.0f;
        granular.setPitchRatio(1.0f);
        return;
    }
    
    pitchRatio = std::max(0.25f, std::min(4.0f, ratio));
    granular.setPitchRatio(pitchRatio);
}

void TimePitchProcessor::setEnabled(bool en)
{
    enabled = en;
}

void TimePitchProcessor::reset()
{
    granular.reset();
}

int TimePitchProcessor::process(const float* in, int inSamples, float* out, int outSamplesWanted)
{
    // Defensive checks
    if (in == nullptr || out == nullptr) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::NULL_BUFFER);
        std::fill(out, out + outSamplesWanted, 0.0f);
        return 0;
    }
    
    if (inSamples <= 0 || outSamplesWanted <= 0) {
        return 0;
    }
    
    // If disabled, just pass through
    if (!enabled) {
        int toCopy = std::min(inSamples, outSamplesWanted);
        std::memcpy(out, in, toCopy * sizeof(float));
        return toCopy;
    }
    
    // Validate pitch ratio
    if (!std::isfinite(pitchRatio) || pitchRatio <= 0.001f) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::BAD_RATIO);
        // Bypass and pass through
        int toCopy = std::min(inSamples, outSamplesWanted);
        std::memcpy(out, in, toCopy * sizeof(float));
        return toCopy;
    }
    
    // Use granular time-warp core
    int finalOutput = granular.process(in, inSamples, out, outSamplesWanted);
    
    // Check output for NaN/Inf
    for (int i = 0; i < finalOutput; ++i) {
        if (!std::isfinite(out[i])) {
            out[i] = 0.0f;
        }
    }
    
    return finalOutput;
}

} // namespace Core

