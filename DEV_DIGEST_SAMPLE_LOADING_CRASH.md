# Developer Digest: Sample Loading Crash Issue

## Problem Summary
The application crashes consistently when attempting to load audio samples (WAV/AIFF files) via the "Load Sample..." button. The crash occurs during the sample loading process, specifically when updating the sample data in the audio engine.

## Technical Context

### Architecture
- **UI Thread**: Handles file loading via `PluginProcessor::loadSampleFromFile()`
- **Audio Thread**: Continuously processes audio via `SamplerEngine::process()`
- **Core Engine**: `SamplerEngine` owns sample data in `ownedSampleData` vector
- **Voices**: `SamplerVoice` instances store raw pointers (`const float* sampleData`) to the engine's sample data

### Data Flow
1. UI thread loads file → `PluginProcessor::loadSampleFromFile()`
2. Creates JUCE `AudioBuffer` from file
3. Calls `JuceEngineAdapter::setSample()` (UI thread)
4. Calls `SamplerEngine::setSample()` (UI thread)
5. Audio thread simultaneously calls `SamplerEngine::process()` → `VoiceManager::process()` → `SamplerVoice::process()`

## Root Cause Analysis

### The Race Condition
The crash is caused by a **race condition** between threads:

1. **UI Thread** calls `SamplerEngine::setSample()` which:
   - Resizes `ownedSampleData` vector (may reallocate memory)
   - Updates all voice pointers via `voiceManager.setSample()`

2. **Audio Thread** simultaneously:
   - Calls `SamplerEngine::process()` → `voiceManager.process()`
   - Voices access `sampleData[idx]` using stored raw pointers
   - If vector was reallocated, pointers become **invalid**, causing crash

### Evidence from Logs
- Logs show "setSample entry" and "temp vector ready"
- Crash occurs before "vector swapped" log entry
- Indicates crash happens during vector resize or pointer update

## Attempted Fixes

### Fix 1: Engine-Owned Copy
**Approach**: Made `SamplerEngine` own its own copy of sample data
- Added `ownedSampleData` vector to `SamplerEngine`
- Engine copies data from adapter, making it independent
- **Result**: Still crashed - race condition persists

### Fix 2: Temporary Vector + Swap
**Approach**: Create temporary vector, then swap atomically
- Create `newSampleData` vector outside critical section
- Swap into `ownedSampleData` using `std::move()`
- **Result**: Still crashed - swap itself may cause issues

### Fix 3: Mutex Protection
**Approach**: Use mutex to synchronize access
- Added `std::mutex sampleDataMutex` to `SamplerEngine`
- Lock mutex in `setSample()` during update
- Lock mutex in `process()` during voice processing
- **Result**: **Deadlock** - audio thread holds lock continuously, blocking UI thread

### Fix 4: Remove Mutex from Process
**Approach**: Only lock in `setSample()`, not in `process()`
- Removed mutex lock from `process()` method
- Kept lock only in `setSample()` for updates
- **Result**: Still crashes - mutex acquisition may be failing or race condition persists

### Fix 5: Lock-Free Approach
**Approach**: Removed mutex entirely, rely on vector swap atomicity
- Removed all mutex code
- Use simple `std::move()` swap
- **Result**: Still crashes

## Current Code State

### Key Files Modified
- `Source/Core/SamplerEngine.h` - Added/removed mutex, sample data storage
- `Source/Core/SamplerEngine.cpp` - Multiple iterations of thread-safety fixes
- `Source/JuceWrapper/JuceEngineAdapter.cpp` - Sample data copying logic

### Current Implementation (`SamplerEngine::setSample`)
```cpp
void SamplerEngine::setSample(const float* data, int length, double sourceSampleRate) {
    // Create temporary vector
    std::vector<float> newSampleData;
    newSampleData.resize(static_cast<size_t>(length));
    std::copy(data, data + length, newSampleData.begin());
    
    // Swap into ownedSampleData
    ownedSampleData = std::move(newSampleData);
    const float* newPtr = ownedSampleData.data();
    
    // Update all voice pointers
    voiceManager.setSample(newPtr, length, sourceSampleRate);
}
```

## Why Current Fixes Don't Work

1. **Vector Reallocation**: When `ownedSampleData` is resized or swapped, it may reallocate memory, invalidating old pointers
2. **Pointer Staleness**: Voices store raw pointers that become invalid when vector reallocates
3. **No Synchronization**: Without proper synchronization, audio thread can read from invalid pointers
4. **Mutex Deadlock**: Locking during audio processing causes deadlocks

## Recommended Solutions

### Option 1: Double-Buffering (Recommended)
**Approach**: Maintain two sample data buffers, use atomic pointer to switch
- Keep two `std::vector<float>` buffers: `sampleDataBuffer[0]` and `sampleDataBuffer[1]`
- Use `std::atomic<int> currentBufferIndex` to indicate active buffer
- UI thread writes to inactive buffer, then atomically swaps index
- Audio thread always reads from `sampleDataBuffer[currentBufferIndex.load()]`
- **Pros**: Lock-free, no deadlocks, minimal latency
- **Cons**: 2x memory usage

### Option 2: Reference Counting
**Approach**: Use shared ownership of sample data
- Use `std::shared_ptr<const std::vector<float>>` for sample data
- Voices store `std::weak_ptr` or `std::shared_ptr`
- Old data remains valid until all voices release it
- **Pros**: Automatic memory management, safe
- **Cons**: Overhead of reference counting

### Option 3: Stop Audio Processing During Load
**Approach**: Pause audio thread during sample updates
- Use a flag to pause audio processing
- UI thread sets flag, waits for audio thread to finish current block
- Update sample data
- Clear flag to resume
- **Pros**: Simple, guaranteed safety
- **Cons**: Audio dropouts during sample loading

### Option 4: Copy-on-Write with Immutable Data
**Approach**: Make sample data immutable, copy when updating
- Store sample data in immutable structure
- When updating, create new copy, atomically swap pointer
- Old copy remains valid until no longer referenced
- **Pros**: Lock-free reads, safe updates
- **Cons**: More complex implementation

## Debugging Recommendations

1. **Add More Instrumentation**:
   - Log when voices access sample data
   - Log vector reallocation events
   - Add stack traces on crash

2. **Use Thread Sanitizer**:
   - Compile with `-fsanitize=thread`
   - Run and check for data race warnings

3. **Check Memory Layout**:
   - Verify vector doesn't reallocate unnecessarily
   - Check if pointers are being invalidated

4. **Test Scenarios**:
   - Load sample while audio is playing
   - Load sample while no audio is playing
   - Rapidly load multiple samples
   - Load very large samples (>100MB)

## Files to Review

- `Source/Core/SamplerEngine.cpp` - Lines 60-130 (setSample implementation)
- `Source/Core/SamplerEngine.cpp` - Lines 180-210 (process implementation)
- `Source/Core/VoiceManager.cpp` - Lines 16-29 (setSample for all voices)
- `Source/Core/SamplerVoice.cpp` - Lines 59-83 (setSample for single voice)
- `Source/Core/SamplerVoice.cpp` - Lines 265-266, 443-444 (sample data access)

## Next Steps

1. **Immediate**: Implement double-buffering solution (Option 1)
2. **Testing**: Verify no crashes with various sample sizes and loading scenarios
3. **Performance**: Measure impact on audio latency
4. **Cleanup**: Remove debug logging once issue is resolved

## Additional Notes

- The issue was working "before" according to user, but we've been debugging crashes for multiple iterations
- All attempts to fix have resulted in crashes or deadlocks
- The core issue is the race condition between UI thread updates and audio thread reads
- A proper synchronization mechanism is required - simple fixes don't work

---

**Status**: 🔴 **CRITICAL** - Application unusable for sample loading
**Priority**: **HIGH** - Blocks core functionality
**Estimated Fix Time**: 2-4 hours for double-buffering implementation


