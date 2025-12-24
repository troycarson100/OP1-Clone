#pragma once

#include <vector>

namespace Core {

/**
 * Pure C++ data structures for visualization
 * No JUCE dependencies - hardware portable
 */

// Color representation (RGBA, 32-bit)
struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    
    Color() : r(255), g(255), b(255), a(255) {}
    Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
};

// Rectangle bounds
struct Rectangle {
    int x;
    int y;
    int width;
    int height;
    
    Rectangle() : x(0), y(0), width(0), height(0) {}
    Rectangle(int x_, int y_, int w, int h) : x(x_), y(y_), width(w), height(h) {}
    
    int getRight() const { return x + width; }
    int getBottom() const { return y + height; }
    int getCentreX() const { return x + width / 2; }
    int getCentreY() const { return y + height / 2; }
};

// Waveform visualization data
struct WaveformData {
    std::vector<float> leftChannel;   // Left channel samples
    std::vector<float> rightChannel;  // Right channel samples (empty if mono)
    
    int startPoint;      // Start sample index
    int endPoint;        // End sample index
    float sampleGain;    // Visual gain scaling
    
    // Loop markers
    int loopStartPoint;
    int loopEndPoint;
    bool loopEnabled;
    
    // Playhead (single - for backward compatibility)
    double playheadPosition;  // Current playback position (sample index) - DEPRECATED, use playheads vector
    float envelopeValue;      // Envelope value for fade visualization - DEPRECATED, use playheads vector
    
    // Multiple playheads (one per active voice)
    struct PlayheadInfo {
        double position;      // Playback position (sample index)
        float envelopeValue;  // Envelope value for this voice
    };
    std::vector<PlayheadInfo> playheads;  // All active voice playheads
    
    // Colors
    Color waveformColor;
    Color backgroundColor;
    Color gridColor;
    Color markerColor;
    Color playheadColor;
    
    WaveformData()
        : startPoint(0)
        , endPoint(0)
        , sampleGain(1.0f)
        , loopStartPoint(0)
        , loopEndPoint(0)
        , loopEnabled(false)
        , playheadPosition(-1.0)
        , envelopeValue(0.0f)
        , waveformColor(255, 255, 255)      // White
        , backgroundColor(10, 10, 10)       // Very dark
        , gridColor(34, 34, 34)            // Subtle grid
        , markerColor(255, 255, 255)       // White
        , playheadColor(255, 255, 0, 120)   // Yellow, ~47% opacity (120/255) - fainter
    {}
};

// ADSR visualization data
struct ADSRData {
    float attackMs;      // Attack time in milliseconds
    float decayMs;       // Decay time in milliseconds
    float sustainLevel;  // Sustain level (0.0-1.0)
    float releaseMs;     // Release time in milliseconds
    
    // Colors
    Color backgroundColor;  // Pill background (white)
    Color borderColor;       // Pill border (dark)
    Color envelopeColor;     // ADSR envelope lines (blue)
    
    ADSRData()
        : attackMs(800.0f)  // Default: 800ms
        , decayMs(0.0f)
        , sustainLevel(1.0f)
        , releaseMs(1000.0f)  // Default: 1000ms (1s)
        , backgroundColor(255, 255, 255)    // White
        , borderColor(51, 51, 51)           // Dark gray
        , envelopeColor(95, 163, 209)       // Blue
    {}
};

} // namespace Core

