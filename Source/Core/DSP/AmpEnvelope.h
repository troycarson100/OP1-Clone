#pragma once

namespace Core {
namespace DSP {

/**
 * Robust, click-free ADSR envelope generator
 * Uses one-pole exponential approach for smooth transitions
 * Portable C++ - no JUCE dependencies
 */
class AmpEnvelope {
public:
    enum class Stage {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };
    
    AmpEnvelope();
    
    // Prepare envelope for audio processing
    void prepare(double sampleRate);
    
    // Set ADSR parameters (in seconds, except sustain which is 0.0-1.0)
    void setParams(float attackSec, float decaySec, float sustain01, float releaseSec);
    
    // Trigger note on (velocity 0.0-1.0)
    void noteOn(float velocity01);
    
    // Trigger note off (starts release phase)
    void noteOff();
    
    // Reset envelope to idle state
    void reset();
    
    // Process one sample, returns gain (0.0-1.0) multiplied by velocity
    float processSample();
    
    // Check if envelope is active (not idle)
    bool isActive() const { return stage != Stage::Idle; }
    
    // Get current envelope value (0.0-1.0, before velocity scaling)
    float getValue() const { return value; }
    
    // Get current stage
    Stage getStage() const { return stage; }
    
    // Get velocity gain (for debugging)
    float getVelocityGain() const { return velGain; }
    
private:
    double sampleRate;
    float value;  // Current envelope value (0.0-1.0)
    float velGain; // Velocity gain multiplier (0.0-1.0)
    
    // Parameters (with smoothing targets)
    float attackSec;
    float decaySec;
    float sustain01;
    float releaseSec;
    
    // Smoothed parameters (current values being used)
    float currentAttackSec;
    float currentDecaySec;
    float currentSustain01;
    float currentReleaseSec;
    
    // One-pole coefficients (computed from time constants)
    float attackCoeff;
    float decayCoeff;
    float releaseCoeff;
    
    // Target values per stage
    float attackTarget;   // Always 1.0
    float decayTarget;    // sustain01
    float releaseTarget; // Always 0.0
    float releaseStartValue; // Value when release phase started (for time scaling)
    
    // Current stage
    Stage stage;
    
    // Parameter smoothing state
    static constexpr float smoothingTimeSec = 0.03f; // 30ms smoothing
    float smoothingCoeff;
    bool needsSmoothing;
    
    // Update coefficients from current time constants
    void updateCoefficients();
    
    // Update only attack and decay coefficients (used during smoothing when in Release stage)
    void updateAttackDecayCoefficients();
    
    // Update release coefficient specifically (called on noteOff with current starting value)
    void updateReleaseCoefficient();
    
    // Smooth parameters toward targets
    void smoothParameters();
    
    // Denormal protection threshold
    static constexpr float denormalThreshold = 1e-12f;
    
    // Helper: clamp coefficient to valid range
    static float clampCoeff(float coeff);
};

} // namespace DSP
} // namespace Core

