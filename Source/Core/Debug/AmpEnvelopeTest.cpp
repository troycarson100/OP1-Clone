#include "AmpEnvelopeTest.h"
#include "../DSP/AmpEnvelopeADSR.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace Core {
namespace Debug {

bool AmpEnvelopeTest::testBasicCycle() {
    printf("=== Test 1: Basic Envelope Cycle ===\n");
    
    DSP::AmpEnvelopeADSR env;
    env.prepare(44100.0);
    env.setParams(0.1f, 0.1f, 0.5f, 0.2f); // 100ms attack, 100ms decay, 0.5 sustain, 200ms release
    
    const int totalSamples = static_cast<int>(44100.0 * 3.0); // 3 seconds total
    float* values = new float[totalSamples];
    
    // noteOn, run 1 second, noteOff, run 2 seconds
    env.noteOn(1.0f);
    
    int noteOffSample = static_cast<int>(44100.0 * 1.0); // 1 second
    
    for (int i = 0; i < totalSamples; ++i) {
        if (i == noteOffSample) {
            env.noteOff();
        }
        values[i] = env.processSample();
    }
    
    // Check for jumps
    int jumpIndex = -1;
    bool hasJumps = checkForJumps(values, totalSamples, 0.1f, jumpIndex);
    
    if (hasJumps) {
        printf("FAIL: Jump detected at sample %d\n", jumpIndex);
        printf("  Value before: %f, Value after: %f\n", 
               jumpIndex > 0 ? values[jumpIndex - 1] : 0.0f,
               values[jumpIndex]);
    } else {
        printf("PASS: No jumps detected\n");
    }
    
    dumpMinMax(values, totalSamples, "Basic Cycle");
    
    delete[] values;
    return !hasJumps;
}

bool AmpEnvelopeTest::testRapidRetrigger() {
    printf("\n=== Test 2: Rapid Retrigger ===\n");
    
    DSP::AmpEnvelopeADSR env;
    env.prepare(44100.0);
    env.setParams(0.01f, 0.01f, 0.5f, 0.01f); // 10ms each
    
    const int onSamples = static_cast<int>(44100.0 * 0.05); // 50ms
    const int offSamples = static_cast<int>(44100.0 * 0.05); // 50ms
    const int cycleSamples = onSamples + offSamples;
    const int numCycles = 20;
    const int totalSamples = cycleSamples * numCycles;
    
    float* values = new float[totalSamples];
    
    bool noteOnState = true;
    int cycleCounter = 0;
    
    for (int i = 0; i < totalSamples; ++i) {
        int cyclePos = i % cycleSamples;
        
        if (cyclePos == 0 && noteOnState) {
            env.noteOn(1.0f);
        } else if (cyclePos == onSamples && noteOnState) {
            env.noteOff();
            noteOnState = false;
        } else if (cyclePos == 0 && !noteOnState) {
            env.noteOn(1.0f);
            noteOnState = true;
            cycleCounter++;
        }
        
        values[i] = env.processSample();
    }
    
    // Check for jumps
    int jumpIndex = -1;
    bool hasJumps = checkForJumps(values, totalSamples, 0.1f, jumpIndex);
    
    if (hasJumps) {
        printf("FAIL: Jump detected at sample %d\n", jumpIndex);
        printf("  Value before: %f, Value after: %f\n",
               jumpIndex > 0 ? values[jumpIndex - 1] : 0.0f,
               values[jumpIndex]);
    } else {
        printf("PASS: No jumps detected\n");
    }
    
    dumpMinMax(values, totalSamples, "Rapid Retrigger");
    
    delete[] values;
    return !hasJumps;
}

void AmpEnvelopeTest::runAllTests() {
    printf("Running AmpEnvelopeADSR Tests...\n\n");
    
    bool test1 = testBasicCycle();
    bool test2 = testRapidRetrigger();
    
    printf("\n=== Test Summary ===\n");
    printf("Test 1 (Basic Cycle): %s\n", test1 ? "PASS" : "FAIL");
    printf("Test 2 (Rapid Retrigger): %s\n", test2 ? "PASS" : "FAIL");
    printf("Overall: %s\n", (test1 && test2) ? "PASS" : "FAIL");
}

bool AmpEnvelopeTest::checkForJumps(const float* values, int numSamples, float threshold, int& jumpIndex) {
    for (int i = 1; i < numSamples; ++i) {
        float delta = std::abs(values[i] - values[i - 1]);
        if (delta > threshold) {
            jumpIndex = i;
            return true;
        }
    }
    jumpIndex = -1;
    return false;
}

void AmpEnvelopeTest::dumpMinMax(const float* values, int numSamples, const char* label) {
    float minVal = values[0];
    float maxVal = values[0];
    
    for (int i = 1; i < numSamples; ++i) {
        minVal = std::min(minVal, values[i]);
        maxVal = std::max(maxVal, values[i]);
    }
    
    printf("%s - Min: %f, Max: %f\n", label, minVal, maxVal);
}

} // namespace Debug
} // namespace Core





