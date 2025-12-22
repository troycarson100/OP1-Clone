#include "MoogLadderFilter.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <chrono>

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
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H5\",\"location\":\"MoogLadderFilter.cpp:41\",\"message\":\"updateCoefficients entry\",\"data\":{\"sampleRate\":" << sampleRate << ",\"cutoffHz\":" << cutoffHz << ",\"resonance\":" << resonance << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // Safety check: ensure sample rate is valid
    if (sampleRate <= 0.0) {
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H5\",\"location\":\"MoogLadderFilter.cpp:45\",\"message\":\"updateCoefficients early return - invalid sampleRate\",\"data\":{\"sampleRate\":" << sampleRate << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
        return;
    }
    
    // Calculate cutoff coefficient g
    // g = 1 - exp(-2 * pi * cutoff / sampleRate)
    const float pi = 3.14159265f;
    float w = 2.0f * pi * cutoffHz / static_cast<float>(sampleRate);
    g = 1.0f - std::exp(-w);
    
    // Clamp g to prevent instability
    g = std::max(0.0f, std::min(1.0f, g));
    
    // Resonance coefficient (feedback amount)
    // Higher resonance = more feedback
    resonanceCoeff = resonance * 3.5f; // Scale resonance to feedback range
    resonanceCoeff = std::max(0.0f, std::min(4.0f, resonanceCoeff));
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H5\",\"location\":\"MoogLadderFilter.cpp:56\",\"message\":\"updateCoefficients exit\",\"data\":{\"g\":" << g << ",\"resonanceCoeff\":" << resonanceCoeff << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
}

float MoogLadderFilter::tanhApprox(float x) const
{
    // Fast tanh approximation for saturation
    // Clamp input to prevent overflow
    x = std::max(-3.0f, std::min(3.0f, x));
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

float MoogLadderFilter::process(float input)
{
    // Safety check: ensure filter is prepared
    if (!isPrepared || sampleRate <= 0.0) {
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"MoogLadderFilter.cpp:67\",\"message\":\"process early return - not prepared\",\"data\":{\"isPrepared\":" << (isPrepared ? 1 : 0) << ",\"sampleRate\":" << sampleRate << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
        return input; // Pass through if not prepared
    }
    
    // Don't bypass - let the filter process even at high frequencies
    // The filter naturally passes more signal at high cutoffs, which is correct behavior
    // Only bypass if cutoff is at absolute maximum (20kHz) AND we want true bypass
    // For now, always process to ensure filter works at all settings
    
    // Moog ladder filter (Stilson/Smith algorithm)
    // 4-pole cascaded one-pole filters with feedback
    
    // Input with resonance feedback
    float in = input - resonanceCoeff * delay[3];
    
    // Apply saturation (tanh) for analog-like behavior
    in = tanhApprox(in);
    
    // 4-stage ladder filter
    float temp = in;
    for (int i = 0; i < 4; ++i) {
        // One-pole low-pass: y = g * x + (1 - g) * y_prev
        stage[i] = g * temp + (1.0f - g) * stage[i];
        temp = stage[i];
        delay[i] = stage[i];
    }
    
    return delay[3]; // Output from last stage
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

