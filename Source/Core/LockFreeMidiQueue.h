#pragma once

#include "MidiEvent.h"
#include <atomic>
#include <cstdint>

namespace Core {

/**
 * Lock-free ring buffer for MIDI events
 * Portable C++ - no JUCE dependencies
 * Single producer (UI/MIDI thread), single consumer (audio thread)
 */
class LockFreeMidiQueue {
public:
    static constexpr int CAPACITY = 64; // Power of 2 for efficient modulo
    
    LockFreeMidiQueue();
    
    /**
     * Push event from UI/MIDI thread (non-blocking)
     * Returns true if successful, false if queue is full
     */
    bool push(const MidiEvent& event);
    
    /**
     * Pop event in audio thread (non-blocking)
     * Returns true if event was popped, false if queue is empty
     */
    bool pop(MidiEvent& event);
    
    /**
     * Get number of events in queue (approximate, for debugging)
     */
    int size() const;
    
    /**
     * Clear all events (audio thread only)
     */
    void clear();
    
private:
    MidiEvent events[CAPACITY];
    std::atomic<uint32_t> writePos{0}; // Write position (UI thread)
    std::atomic<uint32_t> readPos{0};  // Read position (audio thread)
    
    // Helper: get actual index from position
    int getIndex(uint32_t pos) const { return pos & (CAPACITY - 1); }
};

} // namespace Core



