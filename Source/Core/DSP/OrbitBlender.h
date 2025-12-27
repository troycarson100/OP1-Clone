#pragma once

#include <array>
#include <cmath>

namespace Core {
namespace DSP {

/**
 * OrbitBlender - Smoothly blends between 4 sample slots
 * Inspired by Lunacy CUBE orbit mode
 * 
 * Portable C++ - no JUCE dependencies
 * Hardware-ready implementation
 */
class OrbitBlender {
public:
    enum class Shape {
        Circle,      // Circular orbit through 4 corners
        PingPong,    // Ping-pong between slots
        Corners,     // Jump between corners
        RandomSmooth, // Smooth random movement
        Figure8,     // Figure-8 (lemniscate) path
        ZigZag,      // Zig-zag pattern
        Spiral,      // Spiral pattern
        Square       // Square/box pattern
    };
    
    OrbitBlender();
    ~OrbitBlender();
    
    // Set orbit rate in Hz (how fast it cycles)
    void setRateHz(float rateHz);
    
    // Set orbit shape
    void setShape(Shape shape);
    
    // Set which slots are active (bitmask: bit 0 = slot A, bit 1 = slot B, etc.)
    // Only active slots will receive nonzero weights
    void setActiveSlotsMask(uint8_t mask);
    
    // Reset orbit phase to 0
    void reset();
    
    // Update orbit and return weights for 4 slots [A, B, C, D]
    // dtSeconds: time since last update
    // Returns: array of 4 weights, normalized so sum = 1.0
    std::array<float, 4> update(float dtSeconds);
    
    // Get current phase (0.0 to 1.0)
    float getPhase() const { return phase; }
    
private:
    float rateHz;
    Shape shape;
    uint8_t activeSlotsMask;
    
    float phase;  // Current phase (0.0 to 1.0)
    
    // Smoothing per weight (one-pole lowpass for click-free transitions)
    std::array<float, 4> smoothedWeights;
    float smoothingCoeff;  // One-pole coefficient (0.0 to 1.0)
    
    // Random state for RandomSmooth shape
    float randomPhase;
    float randomTarget;
    
    // Compute raw weights for current phase (before smoothing and normalization)
    std::array<float, 4> computeRawWeights(float currentPhase);
    
    // Normalize weights so sum = 1.0 and only active slots have nonzero values
    void normalizeWeights(std::array<float, 4>& weights);
    
    // One-pole smoothing
    float smooth(float current, float target, float coeff);
    
    // Simple PRNG (linear congruential generator)
    uint32_t randomState;
    float randomFloat();
};

} // namespace DSP
} // namespace Core

