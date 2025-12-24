#include "MoogLadderFilter.h"
#include <cmath>
#include <algorithm>

namespace Core {

MoogLadderFilter::MoogLadderFilter()
    : sampleRate(44100.0)
    , cutoffHz(1000.0f)
    , resonance(1.0f)
    , drive(0.0f)
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

void MoogLadderFilter::setCutoffNoUpdate(float cutoff)
{
    cutoffHz = std::max(20.0f, std::min(20000.0f, cutoff));
    // Don't update coefficients - caller will update when ready
}

void MoogLadderFilter::setResonance(float res)
{
    resonance = std::max(0.0f, std::min(4.0f, res));
    updateCoefficients();
}

void MoogLadderFilter::setDrive(float drv)
{
    drive = std::max(0.0f, drv);  // Allow values > 1.0 for more saturation
}

void MoogLadderFilter::updateCoefficients()
{
    // Safety check: ensure sample rate is valid
    if (sampleRate <= 0.0) {
        return;
    }
    
    // Classic Moog ladder filter coefficient calculation
    // Based on Stilson/Smith algorithm - proven implementation
    const float pi = 3.14159265f;
    
    // Calculate normalized frequency
    float w = 2.0f * pi * cutoffHz / static_cast<float>(sampleRate);
    
    // Classic g calculation - simple and proven
    // This gives the characteristic Moog sound
    g = 1.0f - std::exp(-w);
    
    // Clamp g to prevent instability
    g = std::max(0.0f, std::min(1.0f, g));
    
    // Frequency-dependent resonance scaling
    // At high frequencies (high g), excessive resonance scaling causes audio cutout
    // Reduce scaling at high frequencies to prevent this issue
    // At low frequencies (low g), resonance is naturally more effective
    // Scale resonance based on filter response, but cap at high frequencies
    float resonanceScale;
    if (cutoffHz > 10000.0f) {
        // At high frequencies (>10kHz), use minimal scaling to prevent cutout
        // Scale from 1.0 to 1.5 instead of 1.0 to 3.0
        resonanceScale = 1.0f + g * 0.5f;  // Scale from 1.0 to 1.5 based on g
    } else if (cutoffHz > 5000.0f) {
        // At mid-high frequencies (5-10kHz), use moderate scaling
        resonanceScale = 1.0f + g * 1.0f;  // Scale from 1.0 to 2.0 based on g
    } else {
        // At lower frequencies (<5kHz), use full scaling for audible resonance
        resonanceScale = 1.0f + g * 2.0f;  // Scale from 1.0 to 3.0 based on g
    }
    
    // Apply scaled resonance
    // Base resonance (0-4) scaled by frequency response
    resonanceCoeff = resonance * resonanceScale;
    resonanceCoeff = std::max(0.0f, std::min(4.0f, resonanceCoeff));
}

float MoogLadderFilter::tanhApprox(float x) const
{
    // Fast, characterful tanh approximation that actually saturates
    // Clamp input to prevent overflow
    x = std::max(-8.0f, std::min(8.0f, x));
    
    // Fast rational approximation that saturates properly
    // This gives smooth saturation with good character
    float x2 = x * x;
    float absX = std::abs(x);
    
    // Rational approximation: x / (1 + |x|) scaled for tanh-like behavior
    // This ensures proper saturation
    if (absX < 1.0f) {
        // For small values, use polynomial approximation
        return x * (1.0f - x2 / 3.0f);
    } else {
        // For larger values, use saturating function
        float sign = (x >= 0.0f) ? 1.0f : -1.0f;
        return sign * (1.0f - 1.0f / (1.0f + absX));
    }
}

float MoogLadderFilter::saturate(float x) const
{
    // Drive saturation - introduces nice filter drive character
    if (drive <= 0.0f) {
        return x;
    }
    
    // Apply drive - scale input to push into saturation
    // Drive creates harmonic distortion and warmth
    // More aggressive scaling for noticeable saturation even at low drive
    float driveAmount = 1.0f + drive * 4.0f;  // 1.0 to 5.0 range
    float driven = x * driveAmount;
    
    // Apply soft saturation using tanh - this creates the classic Moog drive sound
    // Tanh provides smooth, musical saturation with even harmonics
    float saturated = tanhApprox(driven);
    
    // Add subtle harmonic enhancement for more character
    // This gives the "beefy" Moog filter drive sound
    if (drive > 0.2f) {
        // Add even harmonics for warmth (squared term creates even harmonics)
        float harmonic = saturated * saturated * 0.15f * drive;
        saturated = saturated + harmonic;
        
        // Re-saturate to prevent overshoot
        saturated = tanhApprox(saturated * 1.2f);
    }
    
    // Normalize output level - saturation reduces peak level, so we compensate
    // But keep it musical, not just a volume boost
    float output = saturated * (0.8f + drive * 0.4f);
    
    // Clamp to prevent instability
    output = std::max(-0.98f, std::min(0.98f, output));
    
    return output;
}

float MoogLadderFilter::process(float input)
{
    // Safety check: ensure filter is prepared
    if (!isPrepared || sampleRate <= 0.0) {
        return input; // Pass through if not prepared
    }
    
    // Bypass filter at very high cutoffs (near Nyquist) for clean pass-through
    // CRITICAL: Keep filter state in sync to prevent clicks when switching modes
    // Lowered bypass threshold to 19500 Hz to ensure smooth transition
    bool isBypassed = (cutoffHz >= 19500.0f);
    
    if (isBypassed) {
        // At max cutoff, smoothly track filter state with input
        // This prevents discontinuities when switching from bypass to active
        // Use a very high g value (0.95) to track input quickly but smoothly
        // This ensures delay[3] is close to input, minimizing jumps in resonance feedback
        float syncG = 0.95f;  // High g to track input quickly
        float temp = input;
        for (int i = 0; i < 4; ++i) {
            stage[i] = syncG * temp + (1.0f - syncG) * stage[i];
            temp = stage[i];
            delay[i] = stage[i];
        }
        
        // Output: just apply drive if present, then pass through
        if (drive > 0.0f) {
            return saturate(input);
        }
        return input;
    }
    
    // Classic Moog ladder filter with integrated drive
    // This implementation focuses on character and "beefy" sound
    
    // Input with resonance feedback
    // Classic Moog feedback path - standard implementation
    // Feedback is taken from the last stage (delay[3])
    // This is the correct Moog ladder filter topology
    float feedback = delay[3];
    
    // Apply drive to input BEFORE feedback to prevent instability
    // This prevents drive from interacting with resonance in a way that causes ringing
    float drivenInput = saturate(input);
    
    // Combine driven input with resonance feedback
    float in = drivenInput - resonanceCoeff * feedback;
    
    // Clamp to prevent instability/ringing
    in = std::max(-0.95f, std::min(0.95f, in));
    
    // 4-stage ladder filter
    // Classic one-pole low-pass stages
    float temp = in;
    for (int i = 0; i < 4; ++i) {
        // One-pole low-pass: y[n] = g * x[n] + (1 - g) * y[n-1]
        stage[i] = g * temp + (1.0f - g) * stage[i];
        temp = stage[i];
        delay[i] = stage[i];
    }
    
    // Output from last stage
    // Apply gain compensation for resonance
    // Higher resonance reduces output level, so we compensate
    // This makes resonance audible across the full frequency range
    // At high frequencies with resonance, need more compensation to prevent cutout
    float freqComp;
    if (cutoffHz > 15000.0f) {
        // At very high frequencies (>15kHz), resonance can cause severe cutout
        // Apply stronger compensation to maintain audio level
        freqComp = 1.0f + resonance * 0.8f;
    } else if (cutoffHz > 10000.0f) {
        // At high frequencies (10-15kHz), moderate compensation
        freqComp = 1.0f + resonance * 0.6f;
    } else if (cutoffHz > 3000.0f) {
        // At mid frequencies (3-10kHz), standard compensation
        freqComp = 1.0f + resonance * 0.5f;
    } else {
        // At low frequencies (<3kHz), minimal compensation
        freqComp = 1.0f + resonance * 0.2f;
    }
    
    // Fixed gain compensation - ensure full range has proper gain
    // At high frequencies, need more compensation to prevent silence
    float gainComp = (cutoffHz < 10000.0f) ? 1.1f : 1.15f;  // Increased for high freq to prevent cutout
    return delay[3] * gainComp * freqComp;
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
