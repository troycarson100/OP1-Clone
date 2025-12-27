#include "VoiceManager.h"
#include <algorithm>
#include <fstream>
#include <chrono>

namespace Core {

VoiceManager::VoiceManager()
    : nextVoiceIndex(0)
    , isPolyphonicMode(true)
{
    voicesStartedThisBlock = 0;
}

VoiceManager::~VoiceManager() {
}

// DEPRECATED: setSample() removed - voices now capture sample on noteOn only
// This prevents raw pointer usage and ensures thread safety
// void VoiceManager::setSample(const float* data, int length, double sourceSampleRate) {
//     // DELETED - do not push raw pointers to voices
//     // Sample loading must NOT touch voices at all
//     // Voices capture SampleDataPtr snapshot on noteOn and hold it until voice ends
// }

void VoiceManager::setRootNote(int rootNote) {
    for (auto& voice : voices) {
        voice.setRootNote(rootNote);
    }
}



int VoiceManager::allocateVoice() {
    // First, try to find a free voice (not active)
    // Search through all voices to find an inactive one
    for (int i = 0; i < MAX_VOICES; ++i) {
        int idx = (nextVoiceIndex + i) % MAX_VOICES;
        if (!voices[idx].isActive()) {
            nextVoiceIndex = (idx + 1) % MAX_VOICES;
            return idx;
        }
    }
    
    // All voices active - try to find a voice in release phase to steal (less disruptive)
    for (int i = 0; i < MAX_VOICES; ++i) {
        int idx = (nextVoiceIndex + i) % MAX_VOICES;
        if (voices[idx].isInRelease()) {
            // Steal this voice that's already releasing
            nextVoiceIndex = (idx + 1) % MAX_VOICES;
            return idx;
        }
    }
    
    // No voices in release - steal the quietest voice (lowest envelope value)
    // This minimizes audible artifacts when stealing
    int quietestIdx = -1;
    float quietestEnvelope = 1.0f;
    
    for (int i = 0; i < MAX_VOICES; ++i) {
        int idx = (nextVoiceIndex + i) % MAX_VOICES;
        float env = voices[idx].getEnvelopeValue();
        if (env < quietestEnvelope) {
            quietestIdx = idx;
            quietestEnvelope = env;
        }
    }
    
    if (quietestIdx == -1) {
        quietestIdx = nextVoiceIndex;
    }
    
    // Start fade-out on stolen voice (don't hard-cut)
    voices[quietestIdx].startStealFadeOut();
    nextVoiceIndex = (quietestIdx + 1) % MAX_VOICES;
    return quietestIdx;
}

void VoiceManager::noteOn(int note, float velocity) {
    // Legacy method - call with nullptr sample data and dummy wasStolen
    bool dummy;
    noteOn(note, velocity, nullptr, dummy);
}

bool VoiceManager::noteOn(int note, float velocity, SampleDataPtr sampleData, bool& wasStolen) {
    // Call overloaded version with start delay offset
    return noteOn(note, velocity, sampleData, wasStolen, 0);
}

bool VoiceManager::noteOn(int note, float velocity, SampleDataPtr sampleData, bool& wasStolen, int startDelayOffset) {
    // In mono mode, turn off all currently playing voices
    if (!isPolyphonicMode) {
        for (auto& voice : voices) {
            if (voice.isPlaying()) {
                voice.noteOff(voice.getCurrentNote());
            }
        }
    }
    
    // CRITICAL: Check if there's already a voice that was playing this exact note
    // If so, retrigger that voice (restart from beginning) instead of allocating a new one
    // This provides true retrigger behavior for rapid key presses
    // Check both active voices and voices in release phase (recently played this note)
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].getCurrentNote() == note) {
            // Found a voice that was playing this note (active or in release) - retrigger it
            voices[i].setSampleData(sampleData);
            voices[i].noteOn(note, velocity, startDelayOffset);
            wasStolen = false; // Not stolen, just retriggered
            return true;
        }
    }
    
    // No voice playing this note - allocate a new voice
    int voiceIndex = allocateVoice();
    wasStolen = voices[voiceIndex].isActive() || voices[voiceIndex].isPlaying();
    
    // Increment voice start counter for this block
    voicesStartedThisBlock++;
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"F\",\"location\":\"VoiceManager.cpp:61\",\"message\":\"Voice allocated for note\",\"data\":{\"note\":" << note << ",\"velocity\":" << velocity << ",\"voiceIndex\":" << voiceIndex << ",\"wasActive\":" << (voices[voiceIndex].isActive() ? 1 : 0) << ",\"hasSampleData\":" << (sampleData != nullptr ? 1 : 0) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    // Set sample data snapshot before triggering note
    // Comprehensive validation before setting
    if (!sampleData || sampleData->length <= 0 || 
        sampleData->mono.empty() || sampleData->mono.data() == nullptr) {
        // No valid sample - voice will remain inactive
        // #region agent log
        {
            std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
            if (log.is_open()) {
                log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"F\",\"location\":\"VoiceManager.cpp:80\",\"message\":\"noteOn with invalid sample data\",\"data\":{\"note\":" << note << ",\"sampleDataNull\":" << (sampleData == nullptr ? 1 : 0) << ",\"length\":" << (sampleData ? sampleData->length : 0) << ",\"empty\":" << (sampleData && sampleData->mono.empty() ? 1 : 0) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            }
        }
        // #endregion
        return false; // Don't trigger note if no valid sample
    }
    
    voices[voiceIndex].setSampleData(sampleData);
    
    // Calculate start delay: stagger voices within the block
    int calculatedDelay = (voicesStartedThisBlock * 8) % 64;
    voices[voiceIndex].noteOn(note, velocity, calculatedDelay + startDelayOffset);
    return true;
}

bool VoiceManager::noteOn(int note, float velocity, SampleDataPtr sampleData, bool& wasStolen, int startDelayOffset,
                          float repitchSemitones, int startPoint, int endPoint, float sampleGain,
                          float attackMs, float decayMs, float sustain, float releaseMs,
                          bool loopEnabled, int loopStartPoint, int loopEndPoint) {
    // In mono mode, turn off all currently playing voices
    if (!isPolyphonicMode) {
        for (auto& voice : voices) {
            if (voice.isPlaying()) {
                voice.noteOff(voice.getCurrentNote());
            }
        }
    }
    
    // CRITICAL: Smart retrigger - when same note retriggers, use smooth transition
    // Check if there's already a voice that was playing this exact note
    // If so, retrigger it with smooth fade-in (noteOn preserves slew state when wasActive)
    // This prevents the abrupt cut-off that causes clicks
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].getCurrentNote() == note) {
            // Found a voice that was playing this note (active or in release) - retrigger it
            // noteOn will detect wasActive=true and preserve slew state for smooth transition
            // rampGain and envelope will fade in from 0 over 256 samples
            // Apply slot parameters to this voice
            voices[i].setRepitch(repitchSemitones);
            voices[i].setStartPoint(startPoint);
            voices[i].setEndPoint(endPoint);
            voices[i].setSampleGain(sampleGain);
            voices[i].setAttackTime(attackMs);
            voices[i].setDecayTime(decayMs);
            voices[i].setSustainLevel(sustain);
            voices[i].setReleaseTime(releaseMs);
            voices[i].setLoopEnabled(loopEnabled);
            voices[i].setLoopPoints(loopStartPoint, loopEndPoint);
            voices[i].setSampleData(sampleData);
            // Retrigger - noteOn will handle smooth transition (preserves slew, fades in)
            voices[i].noteOn(note, velocity, startDelayOffset);
            wasStolen = false; // Not stolen, just retriggered
            return true;
        }
    }
    
    // No voice playing this note - allocate a new voice
    int voiceIndex = allocateVoice();
    wasStolen = voices[voiceIndex].isActive() || voices[voiceIndex].isPlaying();
    
    // Increment voice start counter for this block
    voicesStartedThisBlock++;
    
    // Set sample data snapshot before triggering note
    if (!sampleData || sampleData->length <= 0 || 
        sampleData->mono.empty() || sampleData->mono.data() == nullptr) {
        return false; // Don't trigger note if no valid sample
    }
    
    voices[voiceIndex].setSampleData(sampleData);
    
    // Apply slot-specific parameters to this voice BEFORE noteOn
    voices[voiceIndex].setRepitch(repitchSemitones);
    voices[voiceIndex].setStartPoint(startPoint);
    voices[voiceIndex].setEndPoint(endPoint);
    voices[voiceIndex].setSampleGain(sampleGain);
    voices[voiceIndex].setAttackTime(attackMs);
    voices[voiceIndex].setDecayTime(decayMs);
    voices[voiceIndex].setSustainLevel(sustain);
    voices[voiceIndex].setReleaseTime(releaseMs);
    
    // Calculate start delay: stagger voices within the block
    int calculatedDelay = (voicesStartedThisBlock * 8) % 64;
    voices[voiceIndex].noteOn(note, velocity, calculatedDelay + startDelayOffset);
    return true;
}

void VoiceManager::noteOff(int note) {
    // Find voice playing this note and turn it off
    // Use isPlaying() to include voices in release phase
    for (auto& voice : voices) {
        if (voice.isPlaying() && voice.getCurrentNote() == note) {
            voice.noteOff(note);
            break; // Only turn off one voice per note (in case of duplicates)
        }
    }
}

void VoiceManager::process(float** output, int numChannels, int numSamples, double sampleRate) {
    // Reset voice start counter at the beginning of each audio block
    voicesStartedThisBlock = 0;
    // Process all playing voices (including those in release)
    for (auto& voice : voices) {
        if (voice.isPlaying()) {
            voice.process(output, numChannels, numSamples, sampleRate);
        }
    }
}

void VoiceManager::setGain(float gain) {
    for (auto& voice : voices) {
        voice.setGain(gain);
    }
}

void VoiceManager::setVoiceGain(float gain) {
    for (auto& voice : voices) {
        voice.setVoiceGain(gain);
    }
}

void VoiceManager::setADSR(float attackMs, float decayMs, float sustain, float releaseMs) {
    for (auto& voice : voices) {
        voice.setAttackTime(attackMs);
        voice.setDecayTime(decayMs);
        voice.setSustainLevel(sustain);
        voice.setReleaseTime(releaseMs);
    }
}

void VoiceManager::setRepitch(float semitones) {
    for (auto& voice : voices) {
        voice.setRepitch(semitones);
    }
}

void VoiceManager::setStartPoint(int sampleIndex) {
    for (auto& voice : voices) {
        voice.setStartPoint(sampleIndex);
    }
}

void VoiceManager::setEndPoint(int sampleIndex) {
    for (auto& voice : voices) {
        voice.setEndPoint(sampleIndex);
    }
}

void VoiceManager::setSampleGain(float gain) {
    for (auto& voice : voices) {
        voice.setSampleGain(gain);
    }
}

void VoiceManager::setLoopEnabled(bool enabled) {
    for (auto& voice : voices) {
        voice.setLoopEnabled(enabled);
    }
}

void VoiceManager::setLoopPoints(int startPoint, int endPoint) {
    for (auto& voice : voices) {
        voice.setLoopPoints(startPoint, endPoint);
    }
}

void VoiceManager::setWarpEnabled(bool enabled) {
    for (auto& voice : voices) {
        voice.setWarpEnabled(enabled);
    }
}

void VoiceManager::setTimeRatio(double ratio) {
    for (auto& voice : voices) {
        voice.setTimeRatio(ratio);
    }
}

void VoiceManager::setSineTestEnabled(bool enabled) {
    for (auto& voice : voices) {
        voice.setSineTestEnabled(enabled);
    }
}

float VoiceManager::getRepitch() const {
    if (MAX_VOICES > 0) {
        return voices[0].getRepitch();
    }
    return 0.0f;
}

int VoiceManager::getStartPoint() const {
    if (MAX_VOICES > 0) {
        return voices[0].getStartPoint();
    }
    return 0;
}

int VoiceManager::getEndPoint() const {
    if (MAX_VOICES > 0) {
        return voices[0].getEndPoint();
    }
    return 0;
}

float VoiceManager::getSampleGain() const {
    if (MAX_VOICES > 0) {
        return voices[0].getSampleGain();
    }
    return 1.0f;
}

void VoiceManager::getDebugInfo(int& actualInN, int& outN, int& primeRemaining, int& nonZeroCount) const {
    // Get debug info from first active voice
    for (const auto& voice : voices) {
        if (voice.isPlaying()) {
            voice.getDebugInfo(actualInN, outN, primeRemaining, nonZeroCount);
            return;
        }
    }
    // No active voice - return zeros
    actualInN = 0;
    outN = 0;
    primeRemaining = 0;
    nonZeroCount = 0;
}

void VoiceManager::setPolyphonic(bool polyphonic) {
    isPolyphonicMode = polyphonic;
}

int VoiceManager::getActiveVoiceCount() const {
    int count = 0;
    for (const auto& voice : voices) {
        if (voice.isPlaying()) {
            count++;
        }
    }
    return count;
}

double VoiceManager::getPlayheadPosition() const {
    // Find the most recently triggered active voice (highest playhead position)
    // This gives us the "current" playback position for UI display
    double maxPlayhead = -1.0;
    for (const auto& voice : voices) {
        if (voice.isPlaying()) {
            double playhead = voice.getPlayhead();
            if (playhead > maxPlayhead) {
                maxPlayhead = playhead;
            }
        }
    }
    return maxPlayhead;
}

float VoiceManager::getEnvelopeValue() const {
    // Get envelope value from the most recently triggered active voice
    // This is used for fade out visualization
    float maxEnvelope = 0.0f;
    for (const auto& voice : voices) {
        if (voice.isPlaying()) {
            float env = voice.getEnvelopeValue();
            if (env > maxEnvelope) {
                maxEnvelope = env;
            }
        }
    }
    return maxEnvelope;
}

void VoiceManager::getAllActivePlayheads(std::vector<double>& positions, std::vector<float>& envelopeValues) const {
    positions.clear();
    envelopeValues.clear();
    
    for (const auto& voice : voices) {
        if (voice.isPlaying() && voice.getEnvelopeValue() > 0.0f) {
            positions.push_back(voice.getPlayhead());
            envelopeValues.push_back(voice.getEnvelopeValue());
        }
    }
}

} // namespace Core

