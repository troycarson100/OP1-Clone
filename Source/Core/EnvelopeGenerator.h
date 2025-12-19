#pragma once

namespace Core {

/**
 * Envelope Generator (AR - Attack/Release)
 * Portable C++ - no JUCE dependencies
 * 
 * Simple attack/release envelope for modulation purposes
 * Separate from the ADSR envelope used for voice amplitude
 */
class EnvelopeGenerator {
public:
    EnvelopeGenerator();
    ~EnvelopeGenerator();
    
    /**
     * Prepare envelope with sample rate
     */
    void prepare(double sampleRate);
    
    /**
     * Set attack time (milliseconds)
     */
    void setAttack(float attackMs);
    
    /**
     * Set release time (milliseconds)
     */
    void setRelease(float releaseMs);
    
    /**
     * Trigger envelope (start attack phase)
     */
    void trigger();
    
    /**
     * Release envelope (start release phase)
     */
    void release();
    
    /**
     * Process one sample, returns current envelope value (0.0-1.0)
     */
    float process();
    
    /**
     * Process a block of samples, fills output buffer with envelope values
     */
    void processBlock(float* output, int numSamples);
    
    /**
     * Get current envelope value without processing
     */
    float getCurrentValue() const { return currentValue; }
    
    /**
     * Check if envelope is active (not at zero)
     */
    bool isActive() const { return currentValue > 0.001f || state != State::Idle; }
    
    /**
     * Reset envelope to zero
     */
    void reset();

private:
    enum class State {
        Idle,      // Envelope at zero
        Attack,    // Rising to 1.0
        Release    // Falling to 0.0
    };
    
    double sampleRate;
    float attackMs;
    float releaseMs;
    
    State state;
    float currentValue;
    
    // Sample counts for attack and release phases
    int attackSamples;
    int releaseSamples;
    
    // Current position in phase
    int phaseCounter;
    
    void updatePhaseCounts();
    float calculateAttackIncrement() const;
    float calculateReleaseDecrement() const;
};

} // namespace Core

