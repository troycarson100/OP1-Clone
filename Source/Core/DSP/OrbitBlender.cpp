#include "OrbitBlender.h"
#include <algorithm>

namespace Core {
namespace DSP {

OrbitBlender::OrbitBlender()
    : rateHz(1.0f)
    , shape(Shape::Circle)
    , activeSlotsMask(0x0F)  // All 4 slots active by default
    , phase(0.0f)
    , smoothedWeights{0.25f, 0.25f, 0.25f, 0.25f}  // Equal weights initially
    , smoothingCoeff(0.05f)  // Smooth transition (~20 samples at 44.1k for dt=1/44100)
    , randomPhase(0.0f)
    , randomTarget(0.0f)
    , randomState(12345)
{
}

OrbitBlender::~OrbitBlender()
{
}

void OrbitBlender::setRateHz(float rateHz)
{
    this->rateHz = std::max(0.0f, std::min(rateHz, 20.0f));  // Clamp 0-20 Hz
}

void OrbitBlender::setShape(Shape shape)
{
    this->shape = shape;
    // Reset random state when changing shape
    if (shape == Shape::RandomSmooth) {
        randomPhase = 0.0f;
        randomTarget = randomFloat();
    }
}

void OrbitBlender::setActiveSlotsMask(uint8_t mask)
{
    activeSlotsMask = mask & 0x0F;  // Only bottom 4 bits
}

void OrbitBlender::reset()
{
    phase = 0.0f;
    // Reset smoothed weights to equal distribution (only active slots)
    int activeCount = 0;
    for (int i = 0; i < 4; ++i) {
        if (activeSlotsMask & (1 << i)) {
            activeCount++;
        }
    }
    if (activeCount > 0) {
        float weight = 1.0f / static_cast<float>(activeCount);
        for (int i = 0; i < 4; ++i) {
            smoothedWeights[i] = (activeSlotsMask & (1 << i)) ? weight : 0.0f;
        }
    } else {
        smoothedWeights.fill(0.0f);
    }
    randomPhase = 0.0f;
    randomTarget = randomFloat();
}

std::array<float, 4> OrbitBlender::update(float dtSeconds)
{
    // Update phase based on rate
    phase += rateHz * dtSeconds;
    if (phase >= 1.0f) {
        phase -= std::floor(phase);  // Wrap to [0, 1)
    }
    
    // Compute raw weights based on current phase and shape
    std::array<float, 4> rawWeights = computeRawWeights(phase);
    
    // Normalize: only active slots, sum = 1.0
    normalizeWeights(rawWeights);
    
    // Smooth the weights to prevent clicks
    for (int i = 0; i < 4; ++i) {
        smoothedWeights[i] = smooth(smoothedWeights[i], rawWeights[i], smoothingCoeff);
    }
    
    // Re-normalize smoothed weights to ensure sum = 1.0
    normalizeWeights(smoothedWeights);
    
    return smoothedWeights;
}

std::array<float, 4> OrbitBlender::computeRawWeights(float currentPhase)
{
    std::array<float, 4> weights = {0.0f, 0.0f, 0.0f, 0.0f};
    
    switch (shape) {
        case Shape::Circle: {
            // Circular orbit: smoothly transition through 4 corners
            // Phase 0.0 = slot A, 0.25 = slot B, 0.5 = slot C, 0.75 = slot D
            float p = currentPhase * 4.0f;  // 0.0 to 4.0
            int corner = static_cast<int>(p) % 4;
            float t = p - std::floor(p);  // 0.0 to 1.0 within current corner
            
            // Smooth cosine interpolation between corners
            float fadeOut = 0.5f * (1.0f - std::cos(t * 3.14159265f));  // 0 -> 1
            float fadeIn = 1.0f - fadeOut;
            
            weights[corner] = fadeIn;
            weights[(corner + 1) % 4] = fadeOut;
            break;
        }
        
        case Shape::PingPong: {
            // Ping-pong: A -> B -> C -> D -> C -> B -> A -> ...
            float p = currentPhase * 6.0f;  // 0.0 to 6.0 (A->B->C->D->C->B)
            int segment = static_cast<int>(p) % 6;
            float t = p - std::floor(p);
            
            float fadeOut = 0.5f * (1.0f - std::cos(t * 3.14159265f));
            float fadeIn = 1.0f - fadeOut;
            
            switch (segment) {
                case 0: weights[0] = fadeIn; weights[1] = fadeOut; break;  // A -> B
                case 1: weights[1] = fadeIn; weights[2] = fadeOut; break;  // B -> C
                case 2: weights[2] = fadeIn; weights[3] = fadeOut; break;  // C -> D
                case 3: weights[3] = fadeIn; weights[2] = fadeOut; break;  // D -> C
                case 4: weights[2] = fadeIn; weights[1] = fadeOut; break;  // C -> B
                case 5: weights[1] = fadeIn; weights[0] = fadeOut; break;  // B -> A
            }
            break;
        }
        
        case Shape::Corners: {
            // Jump between corners with short crossfades
            float p = currentPhase * 4.0f;  // 0.0 to 4.0
            int corner = static_cast<int>(p) % 4;
            float t = p - std::floor(p);
            
            // Short crossfade window (20% of cycle)
            if (t < 0.2f) {
                // Crossfade from previous corner
                int prevCorner = (corner + 3) % 4;
                float fade = t / 0.2f;  // 0 -> 1
                float fadeOut = 0.5f * (1.0f - std::cos(fade * 3.14159265f));
                float fadeIn = 1.0f - fadeOut;
                weights[prevCorner] = fadeOut;
                weights[corner] = fadeIn;
            } else {
                // Full on current corner
                weights[corner] = 1.0f;
            }
            break;
        }
        
        case Shape::RandomSmooth: {
            // Smooth random movement between slots
            // Update random target based on phase (change target every full cycle)
            float cyclePhase = currentPhase * 2.0f;  // Speed up to 2x for more variation
            if (cyclePhase >= randomPhase + 1.0f || cyclePhase < randomPhase) {
                randomPhase = cyclePhase;
                randomTarget = randomFloat();
            }
            
            // Smoothly interpolate to random target using current phase
            float smoothPhase = 0.5f * (1.0f - std::cos(currentPhase * 3.14159265f * 2.0f));
            
            // Create weights based on random target and smooth interpolation
            // Distribute energy around the random target slot
            int centerSlot = static_cast<int>(randomTarget * 4.0f) % 4;
            float centerWeight = smoothPhase * 0.7f + 0.3f;  // 30% to 100%
            
            // Distribute remaining weight to neighbors
            float remaining = 1.0f - centerWeight;
            weights[centerSlot] = centerWeight;
            
            // Neighbors get some weight
            weights[(centerSlot + 1) % 4] = remaining * 0.4f;
            weights[(centerSlot + 3) % 4] = remaining * 0.4f;
            weights[(centerSlot + 2) % 4] = remaining * 0.2f;
            break;
        }
        
        case Shape::Figure8: {
            // Figure-8 (lemniscate) path - smooth infinity symbol
            // Use parametric lemniscate equation
            const float twoPi = 6.28318530717958647692f;
            const float pi = 3.14159265358979323846f;
            float t = currentPhase * twoPi;
            float a = 1.0f;  // Scale factor
            float denom = 1.0f + std::sin(t) * std::sin(t);
            float x = a * std::cos(t) / denom;
            float y = a * std::sin(t) * std::cos(t) / denom;
            
            // Map from lemniscate coordinates (-a to +a) to slot indices (0-3)
            // Figure-8 crosses center, so map position to nearest corner
            float angle = std::atan2(y, x);
            float distance = std::sqrt(x * x + y * y);
            
            // Map angle to slot: 0°=slot 1 (B), 90°=slot 0 (A), 180°=slot 3 (D), 270°=slot 2 (C)
            int primarySlot = static_cast<int>((angle + pi) / (pi / 2.0f) + 0.5f) % 4;
            if (primarySlot < 0) primarySlot += 4;
            
            // Blend between slots based on distance from center
            float blend = std::min(1.0f, distance / a);
            weights[primarySlot] = blend;
            weights[(primarySlot + 1) % 4] = (1.0f - blend) * 0.5f;
            weights[(primarySlot + 3) % 4] = (1.0f - blend) * 0.5f;
            break;
        }
        
        case Shape::ZigZag: {
            // Zig-zag pattern: A -> C -> B -> D -> A
            float p = currentPhase * 4.0f;  // 0.0 to 4.0
            int segment = static_cast<int>(p) % 4;
            float t = p - std::floor(p);
            
            float fadeOut = 0.5f * (1.0f - std::cos(t * 3.14159265f));
            float fadeIn = 1.0f - fadeOut;
            
            switch (segment) {
                case 0: weights[0] = fadeIn; weights[2] = fadeOut; break;  // A -> C
                case 1: weights[2] = fadeIn; weights[1] = fadeOut; break;  // C -> B
                case 2: weights[1] = fadeIn; weights[3] = fadeOut; break;  // B -> D
                case 3: weights[3] = fadeIn; weights[0] = fadeOut; break;  // D -> A
            }
            break;
        }
        
        case Shape::Spiral: {
            // Spiral pattern - gradually move through all slots
            float p = currentPhase * 4.0f;
            int baseSlot = static_cast<int>(p) % 4;
            float t = p - std::floor(p);
            
            // Spiral creates smooth transitions
            weights[baseSlot] = 1.0f - t;
            weights[(baseSlot + 1) % 4] = t;
            break;
        }
        
        case Shape::Square: {
            // Square/box pattern: A -> B -> C -> D -> A
            float p = currentPhase * 4.0f;
            int corner = static_cast<int>(p) % 4;
            float t = p - std::floor(p);
            
            float fadeOut = 0.5f * (1.0f - std::cos(t * 3.14159265f));
            float fadeIn = 1.0f - fadeOut;
            
            weights[corner] = fadeIn;
            weights[(corner + 1) % 4] = fadeOut;
            break;
        }
    }
    
    return weights;
}

void OrbitBlender::normalizeWeights(std::array<float, 4>& weights)
{
    // Zero out inactive slots
    for (int i = 0; i < 4; ++i) {
        if (!(activeSlotsMask & (1 << i))) {
            weights[i] = 0.0f;
        }
    }
    
    // Normalize so sum = 1.0
    float sum = weights[0] + weights[1] + weights[2] + weights[3];
    if (sum > 0.0001f) {  // Avoid division by zero
        float invSum = 1.0f / sum;
        for (int i = 0; i < 4; ++i) {
            weights[i] *= invSum;
        }
    } else {
        // If no active slots, equal distribution (fallback)
        int activeCount = 0;
        for (int i = 0; i < 4; ++i) {
            if (activeSlotsMask & (1 << i)) {
                activeCount++;
            }
        }
        if (activeCount > 0) {
            float weight = 1.0f / static_cast<float>(activeCount);
            for (int i = 0; i < 4; ++i) {
                weights[i] = (activeSlotsMask & (1 << i)) ? weight : 0.0f;
            }
        } else {
            weights.fill(0.0f);
        }
    }
}

float OrbitBlender::smooth(float current, float target, float coeff)
{
    return current + coeff * (target - current);
}

float OrbitBlender::randomFloat()
{
    // Linear congruential generator
    randomState = randomState * 1103515245U + 12345U;
    return static_cast<float>(randomState & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF);
}

} // namespace DSP
} // namespace Core

