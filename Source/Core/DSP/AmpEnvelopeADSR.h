#pragma once

namespace Core {
namespace DSP {

/**
 * Robust, click-free ADSR envelope generator using piecewise-linear model
 * Portable C++ - no JUCE dependencies
 * 
 * Uses linear increments for predictable knob behavior and parameter smoothing
 * to avoid zipper noise.
 */
class AmpEnvelopeADSR {
public:
    enum class Stage {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };
    
    AmpEnvelopeADSR();
    
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
    float getValue() const { return currentValue; }
    
    // Get current stage
    Stage getStage() const { return stage; }
    
    // Get velocity gain (for debugging)
    float getVelocityGain() const { return velocityGain; }
    
    // Get max delta per block (for debugging)
    float getMaxDeltaPerBlock() const { return maxDeltaPerBlock; }
    
    // Reset max delta counter (call at start of each block)
    void resetMaxDelta() { maxDeltaPerBlock = 0.0f; lastValue = currentValue; }
    
private:
    double sampleRate;
    float currentValue;  // Current envelope value (0.0-1.0)
    float velocityGain; // Velocity gain multiplier (0.0-1.0)
    float lastValue;    // Previous value for delta tracking
    
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
    
    // Linear increments (computed per block when params change)
    float attackInc;
    float decayInc;
    float releaseInc;
    
    // Current stage
    Stage stage;
    
    // Parameter smoothing state
    static constexpr float smoothingTimeSec = 0.02f; // 20ms smoothing
    float smoothingCoeff;
    bool needsSmoothing;
    
    // Debug: max delta per block
    float maxDeltaPerBlock;
    
    // Update increments from current time constants
    void updateIncrements();
    
    // Smooth parameters toward targets
    void smoothParameters();
    
    // Denormal protection threshold
    static constexpr float denormalThreshold = 1e-12f;
    
    // Helper: clamp value to valid range
    static float clamp(float value, float min, float max);
};

} // namespace DSP
} // namespace Core


