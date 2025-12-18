#pragma once

#include <atomic>
#include <cstdint>

namespace Core {

/**
 * Error codes for TimePitchProcessor debugging
 * Updated from audio thread, read from message thread
 */
enum class TimePitchError : int32_t {
    OK = 0,
    WSOLA_UNDERFLOW = 1,
    WSOLA_OOB = 2,
    RESAMPLER_UNDERFLOW = 3,
    RESAMPLER_OOB = 4,
    BAD_RATIO = 5,
    NULL_BUFFER = 6
};

/**
 * Thread-safe error status for debugging
 * Audio thread writes, message thread reads
 */
class TimePitchErrorStatus {
public:
    static TimePitchErrorStatus& getInstance() {
        static TimePitchErrorStatus instance;
        return instance;
    }
    
    void setError(TimePitchError error) {
        errorCode.store(static_cast<int32_t>(error), std::memory_order_relaxed);
    }
    
    TimePitchError getError() const {
        return static_cast<TimePitchError>(errorCode.load(std::memory_order_relaxed));
    }
    
    void clear() {
        errorCode.store(static_cast<int32_t>(TimePitchError::OK), std::memory_order_relaxed);
    }
    
    const char* getErrorString() const {
        switch (getError()) {
            case TimePitchError::OK: return "OK";
            case TimePitchError::WSOLA_UNDERFLOW: return "WSOLA Underflow";
            case TimePitchError::WSOLA_OOB: return "WSOLA Out of Bounds";
            case TimePitchError::RESAMPLER_UNDERFLOW: return "Resampler Underflow";
            case TimePitchError::RESAMPLER_OOB: return "Resampler Out of Bounds";
            case TimePitchError::BAD_RATIO: return "Bad Pitch Ratio";
            case TimePitchError::NULL_BUFFER: return "Null Buffer";
            default: return "Unknown";
        }
    }

private:
    std::atomic<int32_t> errorCode{static_cast<int32_t>(TimePitchError::OK)};
};

} // namespace Core

