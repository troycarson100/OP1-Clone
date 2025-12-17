#pragma once

namespace Core {

// Portable MIDI event structure - no JUCE dependencies
struct MidiEvent {
    enum Type {
        NoteOff = 0,
        NoteOn = 1
    };
    
    Type type;
    int note;           // MIDI note number (0-127)
    float velocity;     // 0.0 to 1.0
    int sampleOffset;   // Sample offset within the current block (0 to blockSize-1)
    
    MidiEvent() : type(NoteOff), note(0), velocity(0.0f), sampleOffset(0) {}
    MidiEvent(Type t, int n, float v, int offset = 0)
        : type(t), note(n), velocity(v), sampleOffset(offset) {}
};

} // namespace Core

