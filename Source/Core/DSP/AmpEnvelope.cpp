#include "AmpEnvelope.h"
#include <cmath>
#include <algorithm>

namespace Core {
namespace DSP {

AmpEnvelope::AmpEnvelope()
    : sampleRate(44100.0)
    , value(0.0f)
    , velGain(1.0f)
    , attackSec(0.001f)
    , decaySec(0.0f)
    , sustain01(1.0f)
    , releaseSec(0.02f)
    , currentAttackSec(0.001f)
    , currentDecaySec(0.0f)
    , currentSustain01(1.0f)
    , currentReleaseSec(0.02f)
    , attackCoeff(0.0f)
    , decayCoeff(0.0f)
    , releaseCoeff(0.0f)
    , attackTarget(1.0f)
    , decayTarget(1.0f)
    , releaseTarget(0.0f)
    , releaseStartValue(1.0f)
    , stage(Stage::Idle)
    , smoothingCoeff(0.0f)
    , needsSmoothing(false)
{
}

void AmpEnvelope::prepare(double sampleRate) {
    this->sampleRate = sampleRate;
    
    // Compute smoothing coefficient (30ms smoothing time)
    smoothingCoeff = std::exp(-1.0 / (smoothingTimeSec * sampleRate));
    smoothingCoeff = clampCoeff(smoothingCoeff);
    
    // Initialize smoothed parameters to current values
    currentAttackSec = attackSec;
    currentDecaySec = decaySec;
    currentSustain01 = sustain01;
    currentReleaseSec = releaseSec;
    
    // Update coefficients
    updateCoefficients();
}

void AmpEnvelope::setParams(float attackSec, float decaySec, float sustain01, float releaseSec) {
    // Clamp parameters to valid ranges
    this->attackSec = std::max(0.0f, attackSec);
    this->decaySec = std::max(0.0f, decaySec);
    this->sustain01 = std::max(0.0f, std::min(1.0f, sustain01));
    this->releaseSec = std::max(0.0f, releaseSec);
    
    // Mark that smoothing is needed
    needsSmoothing = true;
    
    // Update decay target
    decayTarget = this->sustain01;
}

void AmpEnvelope::noteOn(float velocity01) {
    // Clamp velocity
    velGain = std::max(0.0f, std::min(1.0f, velocity01));
    
    // STEP 1.4: Retrigger behavior - restart attack from current value (smooth)
    // Do NOT hard reset to 0 - this prevents clicks on rapid retriggers
    if (stage == Stage::Idle || value < 0.001f) {
        // Voice was idle or very quiet - start from 0
        value = 0.0f;
    }
    // Otherwise, keep current value and transition to attack (smooth)
    
    // Set targets
    attackTarget = 1.0f;
    decayTarget = currentSustain01;
    releaseTarget = 0.0f;
    
    // Transition to attack stage
    stage = Stage::Attack;
    
    // Update coefficients (in case parameters changed)
    updateCoefficients();
}

void AmpEnvelope::noteOff() {
    // STEP 1.3 & STEP 4: Do NOT reset value to 0 on noteOff
    // Release must start from current value to prevent clicks
    if (stage != Stage::Idle && stage != Stage::Release) {
        // Capture current value and transition to release
        // value remains at current level - no jump
        stage = Stage::Release;
        releaseTarget = 0.0f;
        releaseStartValue = value; // Store starting value for release time scaling
        // Recalculate release coefficient with the current starting value
        updateReleaseCoefficient();
    }
}

void AmpEnvelope::reset() {
    value = 0.0f;
    velGain = 1.0f;
    stage = Stage::Idle;
}

float AmpEnvelope::processSample() {
    // Smooth parameters if needed
    if (needsSmoothing) {
        smoothParameters();
    }
    
    // Process envelope based on current stage
    switch (stage) {
        case Stage::Idle:
            // Idle - output 0
            value = 0.0f;
            break;
            
        case Stage::Attack:
            // Attack: value -> 1.0
            // value = value + (target - value) * (1 - coeff)
            value = value + (attackTarget - value) * (1.0f - attackCoeff);
            
            // Check if we've reached attack target (with small threshold)
            if (value >= 0.999f) {
                value = 1.0f;
                stage = Stage::Decay;
                decayTarget = currentSustain01;
                releaseStartValue = 1.0f; // Update release start value when we reach full amplitude
            }
            break;
            
        case Stage::Decay:
            // Decay: value -> sustain
            value = value + (decayTarget - value) * (1.0f - decayCoeff);
            
            // Check if we've reached sustain level (with small threshold)
            if (std::abs(value - decayTarget) < 0.001f) {
                value = decayTarget;
                stage = Stage::Sustain;
                releaseStartValue = value; // Update release start value when we reach sustain
            }
            break;
            
        case Stage::Sustain:
            // Sustain: hold at sustain level
            value = decayTarget; // Keep at sustain level
            releaseStartValue = value; // Keep release start value updated during sustain
            break;
            
        case Stage::Release:
            // Release: value -> 0.0
            // STEP 4: Smooth transition from current value to 0
            // For release, we want the time to be measured from full amplitude (1.0) to 0.0
            // But we start from current value, so we need to scale the coefficient
            // If we start from 0.5 and want 10s release, it should take 10s * (0.5/1.0) = 5s
            // But standard ADSR convention: release time is always from 1.0 to 0.0
            // So we use the same coefficient regardless of starting value
            value = value + (releaseTarget - value) * (1.0f - releaseCoeff);
            
            // Check if we've reached release target (effectively 0)
            // Use a much lower threshold so release takes the full duration
            if (value < 1e-6f) {
                value = 0.0f;
                stage = Stage::Idle;
            }
            break;
    }
    
    // Denormal protection
    if (std::abs(value) < denormalThreshold) {
        value = 0.0f;
    }
    
    // Ensure output is finite
    if (!std::isfinite(value)) {
        value = 0.0f;
    }
    
    // Apply velocity scaling
    float output = value * velGain;
    
    // Final denormal check on output
    if (std::abs(output) < denormalThreshold) {
        output = 0.0f;
    }
    
    return output;
}

void AmpEnvelope::updateCoefficients() {
    // Compute one-pole coefficients from time constants
    // For an envelope, we want to reach ~99% of target in the specified time
    // One-pole: value = value + (target - value) * (1 - coeff)
    // This can be rewritten as: value = value * coeff + target * (1 - coeff)
    // After n samples: value_n = start * coeff^n + target * (1 - coeff^n)
    // To reach 99% of the way: (value_n - target) / (start - target) = 0.01
    // This means: coeff^n = 0.01, so coeff = 0.01^(1/n) = exp(ln(0.01) / n)
    // For 99%: coeff = exp(-4.605 / (T * sampleRate))
    // For 99.9% (what we check for): coeff = exp(-6.908 / (T * sampleRate))
    
    const float timeConstantFactor = 6.908f; // ln(0.001) ≈ -6.908, for 99.9% completion
    
    if (currentAttackSec > 0.0f && sampleRate > 0.0) {
        float timeSamples = static_cast<float>(currentAttackSec * sampleRate);
        if (timeSamples > 0.1f) {
            attackCoeff = std::exp(-timeConstantFactor / timeSamples);
        } else {
            attackCoeff = 0.0f; // Instant attack
        }
    } else {
        attackCoeff = 0.0f; // Instant attack
    }
    attackCoeff = clampCoeff(attackCoeff);
    
    if (currentDecaySec > 0.0f && sampleRate > 0.0) {
        float timeSamples = static_cast<float>(currentDecaySec * sampleRate);
        if (timeSamples > 0.1f) {
            decayCoeff = std::exp(-timeConstantFactor / timeSamples);
        } else {
            decayCoeff = 0.0f; // Instant decay
        }
    } else {
        decayCoeff = 0.0f; // Instant decay
    }
    decayCoeff = clampCoeff(decayCoeff);
    
    if (currentReleaseSec > 0.0f && sampleRate > 0.0) {
        float timeSamples = static_cast<float>(currentReleaseSec * sampleRate);
        if (timeSamples > 0.1f) {
            // For exponential decay, the time constant is independent of starting value
            // So we use the same coefficient regardless of where release starts
            releaseCoeff = std::exp(-timeConstantFactor / timeSamples);
        } else {
            releaseCoeff = 0.0f; // Instant release
        }
    } else {
        releaseCoeff = 0.0f; // Instant release
    }
    releaseCoeff = clampCoeff(releaseCoeff);
}

void AmpEnvelope::updateAttackDecayCoefficients() {
    // Update only attack and decay coefficients (used during smoothing when in Release stage)
    const float timeConstantFactor = 6.908f; // ln(0.001) ≈ -6.908, for 99.9% completion
    
    if (currentAttackSec > 0.0f && sampleRate > 0.0) {
        float timeSamples = static_cast<float>(currentAttackSec * sampleRate);
        if (timeSamples > 0.1f) {
            attackCoeff = std::exp(-timeConstantFactor / timeSamples);
        } else {
            attackCoeff = 0.0f; // Instant attack
        }
    } else {
        attackCoeff = 0.0f; // Instant attack
    }
    attackCoeff = clampCoeff(attackCoeff);
    
    if (currentDecaySec > 0.0f && sampleRate > 0.0) {
        float timeSamples = static_cast<float>(currentDecaySec * sampleRate);
        if (timeSamples > 0.1f) {
            decayCoeff = std::exp(-timeConstantFactor / timeSamples);
        } else {
            decayCoeff = 0.0f; // Instant decay
        }
    } else {
        decayCoeff = 0.0f; // Instant decay
    }
    decayCoeff = clampCoeff(decayCoeff);
}

void AmpEnvelope::updateReleaseCoefficient() {
    // Update release coefficient with the current starting value
    // This is called specifically when noteOff() is triggered
    const float timeConstantFactor = 6.908f; // ln(0.001) ≈ -6.908, for 99.9% completion
    
    if (currentReleaseSec > 0.0f && sampleRate > 0.0) {
        float timeSamples = static_cast<float>(currentReleaseSec * sampleRate);
        if (timeSamples > 0.1f) {
            // For exponential decay, the time constant is independent of starting value
            // So we use the same coefficient regardless of where release starts
            // This means: if release time is 10s, it takes 10s to go from any value to 0
            releaseCoeff = std::exp(-timeConstantFactor / timeSamples);
        } else {
            releaseCoeff = 0.0f; // Instant release
        }
    } else {
        releaseCoeff = 0.0f; // Instant release
    }
    releaseCoeff = clampCoeff(releaseCoeff);
}

void AmpEnvelope::smoothParameters() {
    // Smooth parameters toward targets (30ms smoothing)
    currentAttackSec = currentAttackSec * smoothingCoeff + attackSec * (1.0f - smoothingCoeff);
    currentDecaySec = currentDecaySec * smoothingCoeff + decaySec * (1.0f - smoothingCoeff);
    currentSustain01 = currentSustain01 * smoothingCoeff + sustain01 * (1.0f - smoothingCoeff);
    currentReleaseSec = currentReleaseSec * smoothingCoeff + releaseSec * (1.0f - smoothingCoeff);
    
    // Update coefficients with smoothed values
    // BUT: if we're in Release stage, don't overwrite the release coefficient
    // (it was calculated correctly in noteOff() with the starting value)
    if (stage != Stage::Release) {
        updateCoefficients();
    } else {
        // Only update attack and decay coefficients, not release
        updateAttackDecayCoefficients();
    }
    
    // Update decay target if sustain changed
    if (stage == Stage::Decay || stage == Stage::Sustain) {
        decayTarget = currentSustain01;
    }
    
    // Check if smoothing is complete (within threshold)
    const float threshold = 0.001f;
    if (std::abs(currentAttackSec - attackSec) < threshold &&
        std::abs(currentDecaySec - decaySec) < threshold &&
        std::abs(currentSustain01 - sustain01) < threshold &&
        std::abs(currentReleaseSec - releaseSec) < threshold) {
        needsSmoothing = false;
    }
}

float AmpEnvelope::clampCoeff(float coeff) {
    // Clamp coefficient to valid range [0, 1)
    return std::max(0.0f, std::min(0.9999f, coeff));
}

} // namespace DSP
} // namespace Core

