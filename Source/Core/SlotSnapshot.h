#pragma once

#include <vector>
#include <string>

// Snapshot of all knob values for a single slot
// Stores parameter values that can be saved/restored per slot
namespace Core {
    struct SlotSnapshot {
        // Sample editing parameters
        float repitchSemitones;
        int startPoint;
        int endPoint;
        float sampleGain;
        
        // Filter parameters (shift mode)
        float lpCutoffHz;
        float lpResonance;
        float lpDriveDb;
        
        // ADSR parameters
        float attackMs;
        float decayMs;
        float sustain;
        float releaseMs;
        
        // Loop parameters
        int loopStartPoint;
        int loopEndPoint;
        bool loopEnabled;
        
        // Playback mode
        bool isPolyphonic;
        
        // Note: playbackMode is now global, not per-slot (removed from SlotSnapshot)
        
        // Sample name
        std::string sampleName;
        
        // Default constructor
        SlotSnapshot()
            : repitchSemitones(0.0f)
            , startPoint(0)
            , endPoint(0)
            , sampleGain(1.0f)
            , lpCutoffHz(20000.0f)
            , lpResonance(1.0f)
            , lpDriveDb(0.0f)
            , attackMs(800.0f)
            , decayMs(0.0f)
            , sustain(1.0f)
            , releaseMs(1000.0f)
            , loopStartPoint(0)
            , loopEndPoint(0)
            , loopEnabled(false)
            , isPolyphonic(true)
            , sampleName("Default (440Hz tone)")
        {
        }
    };
}

