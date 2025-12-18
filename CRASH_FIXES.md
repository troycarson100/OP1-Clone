# Crash Fixes and Defensive Programming

## Summary
Added comprehensive defensive checks and error reporting to prevent crashes in WSOLA/Resampler time-stretching code.

## Files Changed

### 1. `Source/Core/TimePitchError.h` (NEW)
- Thread-safe error status system using atomic integers
- Error codes: OK, WSOLA_UNDERFLOW, WSOLA_OOB, RESAMPLER_UNDERFLOW, RESAMPLER_OOB, BAD_RATIO, NULL_BUFFER
- Safe to read from message thread, write from audio thread

### 2. `Source/Core/WSOLA.cpp`
**Changes:**
- Added null pointer checks at process() entry
- Validate timeScale for NaN/Inf, clamp to 1.0f if invalid
- Added bounds checking before correlation search:
  - Clamp seek range to valid offsets given current ring buffer size
  - Verify candidate positions are within bounds before accessing
  - Added +4 sample safety margin to required input calculation
- Added NaN/Inf protection after correlation, windowing, and overlap-add
- Validate peek() results before using data
- Set error codes when guard conditions trigger

**Key Fixes:**
- Prevented out-of-bounds reads in correlation search loop
- Added safety margin to prevent buffer underflow
- Protected against invalid timeScale values

### 3. `Source/Core/Resampler.cpp`
**Changes:**
- Added null pointer checks at process() entry
- Validate ratio for NaN/Inf, clamp to 1.0f if invalid
- Simplified to per-block linear interpolation (removed complex ring buffer)
- Clamp all indices to valid range (never read negative or beyond input)
- Added NaN/Inf protection on output samples
- Set error codes when guard conditions trigger

**Key Fixes:**
- Removed complex ring buffer that was causing crashes
- All array accesses are bounds-checked
- Fallback to pass-through if ratio is invalid

### 4. `Source/Core/TimePitchProcessor.cpp`
**Changes:**
- Added null pointer checks at process() entry
- Validate pitchRatio for NaN/Inf
- Check output samples for NaN/Inf before returning
- Set error codes when guard conditions trigger
- Bypass processing and output silence/passthrough on errors

### 5. `Source/JuceWrapper/PluginEditor.h/.cpp`
**Changes:**
- Added error status label to UI
- Implemented Timer callback to update error status every 100ms
- Error status displayed in red if error, green if OK
- Safe message-thread read of atomic error code

## AddressSanitizer Instructions

To enable AddressSanitizer for debugging:

1. Open Xcode project (if using Xcode)
2. Select the Op1Clone_Standalone scheme
3. Edit Scheme → Run → Diagnostics
4. Enable:
   - ✅ Address Sanitizer
   - ✅ Undefined Behavior Sanitizer (optional)
5. Build and run in Debug configuration

This will pinpoint exact out-of-bounds locations if crashes still occur.

## Error Codes

- **0 (OK)**: No errors
- **1 (WSOLA_UNDERFLOW)**: Not enough input samples in WSOLA ring buffer
- **2 (WSOLA_OOB)**: Out-of-bounds access attempted in WSOLA
- **3 (RESAMPLER_UNDERFLOW)**: Not enough input samples in Resampler
- **4 (RESAMPLER_OOB)**: Out-of-bounds access attempted in Resampler
- **5 (BAD_RATIO)**: Invalid pitch/time ratio (NaN, Inf, or <= 0.001)
- **6 (NULL_BUFFER)**: Null pointer detected in buffer parameter

## Likely Bug Locations (Fixed)

1. **WSOLA correlation search**: Was searching offsets that could push candidate position negative or beyond ring buffer
   - **Fix**: Clamp search range to valid offsets based on current buffer size

2. **Resampler ring buffer**: Complex ring buffer logic with multiple indices was error-prone
   - **Fix**: Simplified to per-block linear interpolation with bounds checking

3. **Missing input validation**: No checks for NaN/Inf in ratios or buffer pointers
   - **Fix**: Validate all inputs at process() entry, clamp invalid values

4. **Array bounds**: No bounds checking before array access
   - **Fix**: Clamp all indices to valid range, verify buffer sizes before access

## Testing Checklist

- [x] App no longer crashes when pressing keys
- [x] Error status displays in UI (green = OK, red = error)
- [x] All buffer accesses are bounds-checked
- [x] NaN/Inf values are detected and handled
- [x] Invalid ratios are clamped to safe values
- [x] Null pointers are detected and bypassed

## Next Steps

If crashes still occur:
1. Check error status label in UI to see which guard triggered
2. Enable AddressSanitizer in Debug build to get exact line number
3. Check console for any additional error messages

