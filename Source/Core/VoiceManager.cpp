#include "VoiceManager.h"
#include <algorithm>

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

} // namespace Core

