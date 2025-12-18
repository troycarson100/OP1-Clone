#include "VoiceManager.h"
#include <algorithm>
#include <fstream>
#include <chrono>

namespace Core {

VoiceManager::VoiceManager()
    : nextVoiceIndex(0)
{
}

VoiceManager::~VoiceManager() {
}

void VoiceManager::setSample(const float* data, int length, double sourceSampleRate) {
    for (auto& voice : voices) {
        voice.setSample(data, length, sourceSampleRate);
    }
}

void VoiceManager::setRootNote(int rootNote) {
    for (auto& voice : voices) {
        voice.setRootNote(rootNote);
    }
}

void VoiceManager::setWarpEnabled(bool enabled) {
    for (auto& voice : voices) {
        voice.setWarpEnabled(enabled);
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
    
    // All voices active - steal the oldest (round-robin)
    // This ensures we can always play a new note, even if all voices are in use
    int stolen = nextVoiceIndex;
    nextVoiceIndex = (nextVoiceIndex + 1) % MAX_VOICES;
    return stolen;
}

void VoiceManager::noteOn(int note, float velocity) {
    int voiceIndex = allocateVoice();
    
    // #region agent log
    {
        std::ofstream log("/Users/troycarson/Documents/JUCE Projects/OP1-Clone/.cursor/debug.log", std::ios::app);
        if (log.is_open()) {
            log << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"F\",\"location\":\"VoiceManager.cpp:50\",\"message\":\"Voice allocated for note\",\"data\":{\"note\":" << note << ",\"velocity\":" << velocity << ",\"voiceIndex\":" << voiceIndex << ",\"wasActive\":" << (voices[voiceIndex].isActive() ? 1 : 0) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        }
    }
    // #endregion
    
    voices[voiceIndex].noteOn(note, velocity);
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

} // namespace Core

