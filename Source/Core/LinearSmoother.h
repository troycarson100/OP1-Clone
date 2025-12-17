#pragma once

namespace Core {

// Simple linear smoother for parameter changes - portable, no JUCE
// Prevents zipper noise and clicks
class LinearSmoother {
public:
    LinearSmoother() : currentValue(0.0f), targetValue(0.0f), stepSize(0.0f), samplesRemaining(0) {}
    
    void setTarget(float target, int numSamples) {
        targetValue = target;
        if (numSamples > 0) {
            stepSize = (targetValue - currentValue) / static_cast<float>(numSamples);
            samplesRemaining = numSamples;
        } else {
            currentValue = targetValue;
            samplesRemaining = 0;
        }
    }
    
    void setValueImmediate(float value) {
        currentValue = value;
        targetValue = value;
        stepSize = 0.0f;
        samplesRemaining = 0;
    }
    
    float getNextValue() {
        if (samplesRemaining > 0) {
            currentValue += stepSize;
            --samplesRemaining;
            if (samplesRemaining == 0) {
                currentValue = targetValue;
            }
        } else {
            currentValue = targetValue;
        }
        return currentValue;
    }
    
    float getCurrentValue() const {
        return currentValue;
    }
    
    bool isSmoothing() const {
        return samplesRemaining > 0;
    }
    
private:
    float currentValue;
    float targetValue;
    float stepSize;
    int samplesRemaining;
};

} // namespace Core

