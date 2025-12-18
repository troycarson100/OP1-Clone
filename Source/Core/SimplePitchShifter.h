#pragma once

namespace Core {

/**
 * Simple time-stretching pitch shifter
 * Uses windowed overlap-add method for smooth pitch shifting
 * Portable C++ - no JUCE dependencies
 */
class SimplePitchShifter {
public:
    SimplePitchShifter();
    ~SimplePitchShifter();
    
    // Set pitch ratio (1.0 = original pitch, 2.0 = octave up, 0.5 = octave down)
    void setPitchRatio(double ratio);
    
    // Process a single sample with pitch shifting
    // input: input sample value
    // Returns: pitch-shifted output sample
    float process(float input);
    
    // Reset internal state
    void reset();

private:
    double pitchRatio;
    
    // Circular buffer for windowed processing
    static constexpr int BUFFER_SIZE = 2048;
    static constexpr int HOP_SIZE = 512;
    float buffer[BUFFER_SIZE];
    int writePos;
    int readPos;
    double phase;
    
    // Window function (Hanning window)
    float window[BUFFER_SIZE];
    void initWindow();
    
    // Helper: wrap index
    int wrapIndex(int index) const;
};

} // namespace Core

