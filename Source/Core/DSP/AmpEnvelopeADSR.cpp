#include "AmpEnvelopeADSR.h"
#include <cmath>
#include <algorithm>

namespace Core {
namespace DSP {

AmpEnvelopeADSR::AmpEnvelopeADSR()
    : sampleRate(44100.0)
    , currentValue(0.0f)
    , velocityGain(1.0f)
    , lastValue(0.0f)
    , attackSec(0.001f)
    , decaySec(0.0f)
    , sustain01(1.0f)
    , releaseSec(0.02f)
    , currentAttackSec(0.001f)
    , currentDecaySec(0.0f)
    , currentSustain01(1.0f)
    , currentReleaseSec(0.02f)
    , attackInc(0.0f)
    , decayInc(0.0f)
    , releaseInc(0.0f)
    , stage(Stage::Idle)
    , smoothingCoeff(0.0f)
    , needsSmoothing(false)
    , maxDeltaPerBlock(0.0f)
{
}

void AmpEnvelopeADSR::prepare(double sampleRate) {
    this->sampleRate = sampleRate;
    
    // Compute smoothing coefficient (20ms smoothing time)
    smoothingCoeff = std::exp(-1.0 / (smoothingTimeSec * sampleRate));
    
    // Initialize smoothed parameters to current values
    currentAttackSec = attackSec;
    currentDecaySec = decaySec;
    currentSustain01 = sustain01;
    currentReleaseSec = releaseSec;
    
    // Update increments
    updateIncrements();
}

void AmpEnvelopeADSR::setParams(float attackSec, float decaySec, float sustain01, float releaseSec) {
    // Clamp parameters to valid ranges
    this->attackSec = std::max(0.0f, attackSec);
    this->decaySec = std::max(0.0f, decaySec);
    this->sustain01 = clamp(sustain01, 0.0f, 1.0f);
    this->releaseSec = std::max(0.0f, releaseSec);
    
    // Mark that smoothing is needed
    needsSmoothing = true;
}

void AmpEnvelopeADSR::noteOn(float velocity01) {
    velocityGain = clamp(velocity01, 0.0f, 1.0f);
    
    // If stage == Release or stage == Idle: start Attack from currentValue (do NOT reset to 0)
    if (stage == Stage::Release || stage == Stage::Idle) {
        // Start Attack from current value (smooth retrigger)
        stage = Stage::Attack;
        updateIncrements();
    } else {
        // Already in Attack/Decay/Sustain - restart from current value
        stage = Stage::Attack;
        updateIncrements();
    }
}

void AmpEnvelopeADSR::noteOff() {
    // If stage != Idle: stage = Release
    if (stage != Stage::Idle) {
        stage = Stage::Release;
        // Recompute release increment from current value
        updateIncrements();
    }
}

void AmpEnvelopeADSR::reset() {
    currentValue = 0.0f;
    velocityGain = 1.0f;
    lastValue = 0.0f;
    stage = Stage::Idle;
}

float AmpEnvelopeADSR::processSample() {
    // Smooth parameters if needed
    if (needsSmoothing) {
        smoothParameters();
    }
    
    // Store previous value for delta tracking
    float prevValue = currentValue;
    
    // Process envelope based on current stage
    switch (stage) {
        case Stage::Idle:
            currentValue = 0.0f;
            break;
            
        case Stage::Attack:
            // Attack: currentValue += attackInc; if >=1 -> currentValue=1, stage=Decay
            currentValue += attackInc;
            if (currentValue >= 1.0f) {
                currentValue = 1.0f;
                stage = Stage::Decay;
                updateIncrements(); // Recompute decay increment
            }
            break;
            
        case Stage::Decay:
            // Decay: currentValue -= decayInc; if <=sustain -> currentValue=sustain, stage=Sustain
            currentValue -= decayInc;
            if (currentValue <= currentSustain01) {
                currentValue = currentSustain01;
                stage = Stage::Sustain;
            }
            break;
            
        case Stage::Sustain:
            // Sustain: currentValue = sustain
            currentValue = currentSustain01;
            break;
            
        case Stage::Release:
            // Release: currentValue -= releaseInc; if <=0 -> currentValue=0, stage=Idle
            currentValue -= releaseInc;
            if (currentValue <= 0.0f) {
                currentValue = 0.0f;
                stage = Stage::Idle;
            }
            break;
    }
    
    // Track max delta per block
    float delta = std::abs(currentValue - prevValue);
    if (delta > maxDeltaPerBlock) {
        maxDeltaPerBlock = delta;
    }
    
    // Denormal protection
    if (std::abs(currentValue) < denormalThreshold) {
        currentValue = 0.0f;
    }
    
    // NaN/Inf guards
    if (!std::isfinite(currentValue)) {
        currentValue = 0.0f;
        stage = Stage::Idle;
    }
    
    // Apply velocity scaling
    float output = currentValue * velocityGain;
    
    // Final denormal check on output
    if (std::abs(output) < denormalThreshold) {
        output = 0.0f;
    }
    
    // Final NaN check
    if (!std::isfinite(output)) {
        output = 0.0f;
    }
    
    return output;
}

void AmpEnvelopeADSR::updateIncrements() {
    // Compute linear increments based on current stage and smoothed parameters
    float sr = static_cast<float>(sampleRate);
    
    // Attack increment: (1.0 - currentValue) / max(1, attackSec*sr)
    if (currentAttackSec <= 0.0005f) {
        // Instant attack
        attackInc = 1.0f - currentValue; // Reach 1.0 in 1 sample
    } else {
        float attackSamples = currentAttackSec * sr;
        attackInc = (1.0f - currentValue) / std::max(1.0f, attackSamples);
    }
    
    // Decay increment: (1.0 - sustain) / max(1, decaySec*sr)
    if (currentDecaySec <= 0.0005f) {
        // Instant decay
        decayInc = 1.0f - currentSustain01; // Reach sustain in 1 sample
    } else {
        float decaySamples = currentDecaySec * sr;
        decayInc = (1.0f - currentSustain01) / std::max(1.0f, decaySamples);
    }
    
    // Release increment: (currentValue - 0.0) / max(1, releaseSec*sr)
    // IMPORTANT: releaseInc must be computed from CURRENT VALUE at moment of noteOff
    // or when entering Release, so release always smoothly ramps to 0
    if (currentReleaseSec <= 0.0005f) {
        // Instant release
        releaseInc = currentValue; // Reach 0.0 in 1 sample
    } else {
        float releaseSamples = currentReleaseSec * sr;
        releaseInc = currentValue / std::max(1.0f, releaseSamples);
    }
}

void AmpEnvelopeADSR::smoothParameters() {
    // Smooth parameters toward targets (20ms smoothing)
    currentAttackSec = currentAttackSec * smoothingCoeff + attackSec * (1.0f - smoothingCoeff);
    currentDecaySec = currentDecaySec * smoothingCoeff + decaySec * (1.0f - smoothingCoeff);
    currentSustain01 = currentSustain01 * smoothingCoeff + sustain01 * (1.0f - smoothingCoeff);
    currentReleaseSec = currentReleaseSec * smoothingCoeff + releaseSec * (1.0f - smoothingCoeff);
    
    // Update increments when smoothed params move enough or when stage enters a new phase
    // We update increments every sample during smoothing to ensure smooth transitions
    updateIncrements();
    
    // Check if smoothing is complete (within threshold)
    float threshold = 0.001f;
    if (std::abs(currentAttackSec - attackSec) < threshold &&
        std::abs(currentDecaySec - decaySec) < threshold &&
        std::abs(currentSustain01 - sustain01) < threshold &&
        std::abs(currentReleaseSec - releaseSec) < threshold) {
        needsSmoothing = false;
    }
}

float AmpEnvelopeADSR::clamp(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}

} // namespace DSP
} // namespace Core





