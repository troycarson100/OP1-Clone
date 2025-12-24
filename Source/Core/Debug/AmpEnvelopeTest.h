#pragma once

namespace Core {
namespace Debug {

/**
 * Offline test harness for AmpEnvelopeADSR
 * Tests envelope behavior without real-time audio constraints
 */
class AmpEnvelopeTest {
public:
    // Test 1: Basic envelope cycle
    // noteOn, run 1 second, noteOff, run 2 seconds
    // Returns true if test passes (no jumps > 0.1 between samples)
    static bool testBasicCycle();
    
    // Test 2: Rapid retrigger
    // noteOn for 50ms, noteOff for 50ms, repeat 20 times
    // Returns true if test passes (no jumps)
    static bool testRapidRetrigger();
    
    // Run all tests and print results
    static void runAllTests();
    
private:
    // Helper: check for jumps in envelope values
    static bool checkForJumps(const float* values, int numSamples, float threshold, int& jumpIndex);
    
    // Helper: dump min/max values
    static void dumpMinMax(const float* values, int numSamples, const char* label);
};

} // namespace Debug
} // namespace Core



