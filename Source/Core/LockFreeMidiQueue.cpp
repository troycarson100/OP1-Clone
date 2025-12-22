#include "LockFreeMidiQueue.h"

namespace Core {

LockFreeMidiQueue::LockFreeMidiQueue() {
    // Initialize events
    for (int i = 0; i < CAPACITY; ++i) {
        events[i] = MidiEvent();
    }
}

bool LockFreeMidiQueue::push(const MidiEvent& event) {
    uint32_t currentWrite = writePos.load(std::memory_order_relaxed);
    uint32_t nextWrite = currentWrite + 1;
    uint32_t currentRead = readPos.load(std::memory_order_acquire);
    
    // Check if queue is full (writePos + 1 == readPos)
    if (getIndex(nextWrite) == getIndex(currentRead)) {
        return false; // Queue full
    }
    
    // Write event
    events[getIndex(currentWrite)] = event;
    
    // Update write position (release ensures event is written before position update)
    writePos.store(nextWrite, std::memory_order_release);
    return true;
}

bool LockFreeMidiQueue::pop(MidiEvent& event) {
    uint32_t currentRead = readPos.load(std::memory_order_relaxed);
    uint32_t currentWrite = writePos.load(std::memory_order_acquire);
    
    // Check if queue is empty
    if (getIndex(currentRead) == getIndex(currentWrite)) {
        return false; // Queue empty
    }
    
    // Read event
    event = events[getIndex(currentRead)];
    
    // Update read position (release ensures event is read before position update)
    readPos.store(currentRead + 1, std::memory_order_release);
    return true;
}

int LockFreeMidiQueue::size() const {
    uint32_t w = writePos.load(std::memory_order_acquire);
    uint32_t r = readPos.load(std::memory_order_acquire);
    int diff = static_cast<int>(w - r);
    return (diff < 0) ? (diff + CAPACITY) : diff;
}

void LockFreeMidiQueue::clear() {
    // Audio thread only - just reset read position to write position
    uint32_t w = writePos.load(std::memory_order_acquire);
    readPos.store(w, std::memory_order_release);
}

} // namespace Core


