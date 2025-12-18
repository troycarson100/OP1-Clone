#include "WSOLA.h"
#include "Window.h"
#include "TimePitchError.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cfloat>  // For FLT_EPSILON, isfinite

namespace Core {

WSOLA::WSOLA()
    : sampleRate(44100.0)
    , timeScale(1.0f)
    , inputBufferStorage(nullptr)
    , inputBufferCapacity(0)
    , olaBuffer(nullptr)
    , prevTail(nullptr)
    , tempFrame(nullptr)
    , window(nullptr)
    , basePos(0)
{
}

WSOLA::~WSOLA()
{
    deallocateBuffers();
}

void WSOLA::prepare(double rate)
{
    sampleRate = rate;
    allocateBuffers();
    reset();
}

void WSOLA::setTimeScale(float scale)
{
    // Clamp to reasonable range and check for invalid values
    if (!std::isfinite(scale) || scale <= 0.001f) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::BAD_RATIO);
        timeScale = 1.0f;
        return;
    }
    timeScale = std::max(0.25f, std::min(4.0f, scale));
}

void WSOLA::reset()
{
    inputBuffer.reset();
    basePos = 0;
    
    if (olaBuffer != nullptr) {
        std::fill(olaBuffer, olaBuffer + FRAME_SIZE, 0.0f);
    }
    
    if (prevTail != nullptr) {
        std::fill(prevTail, prevTail + OVERLAP, 0.0f);
    }
}

int WSOLA::process(const float* in, int inCount, float* out, int outCapacity)
{
    // Defensive checks
    if (in == nullptr || out == nullptr) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::NULL_BUFFER);
        std::fill(out, out + outCapacity, 0.0f);
        return 0;
    }
    
    if (inCount <= 0 || outCapacity <= 0) {
        return 0;
    }
    
    // Check if buffers are initialized
    if (inputBufferStorage == nullptr || olaBuffer == nullptr || 
        tempFrame == nullptr || window == nullptr) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::NULL_BUFFER);
        std::fill(out, out + outCapacity, 0.0f);
        return 0;
    }
    
    // Validate timeScale
    if (!std::isfinite(timeScale) || timeScale <= 0.001f) {
        TimePitchErrorStatus::getInstance().setError(TimePitchError::BAD_RATIO);
        // Bypass and pass through input
        int toCopy = std::min(inCount, outCapacity);
        std::memcpy(out, in, toCopy * sizeof(float));
        return toCopy;
    }
    
    // Push input into ring buffer
    inputBuffer.push(in, inCount);
    
    int outputSamples = 0;
    int synthesisHop = static_cast<int>(static_cast<float>(ANALYSIS_HOP) * timeScale);
    if (synthesisHop < 1) synthesisHop = 1;
    
    // Process frames until we can't produce more output or run out of input
    while (outputSamples < outCapacity) {
        // Check if we have enough input for a frame
        // Need: basePos + ANALYSIS_HOP + SEEK_RANGE + FRAME_SIZE + 4 (extra safety)
        int neededInput = basePos + ANALYSIS_HOP + SEEK_RANGE + FRAME_SIZE + 4;
        if (inputBuffer.size() < neededInput) {
            if (outputSamples == 0) {
                // First frame - not enough input yet
                TimePitchErrorStatus::getInstance().setError(TimePitchError::WSOLA_UNDERFLOW);
            }
            // Not enough input yet - but if we have some output ready, emit it
            // This helps reduce latency
            if (outputSamples > 0) {
                break;
            }
            // On first frame, we need to wait for enough input
            // But try to produce at least some output by using a simpler method
            if (inputBuffer.size() >= FRAME_SIZE && basePos + FRAME_SIZE <= inputBuffer.size()) {
                // Simple overlap-add without correlation search (lower quality but works immediately)
                int peeked = inputBuffer.peek(tempFrame, FRAME_SIZE, basePos);
                if (peeked < FRAME_SIZE) {
                    break; // Not enough data
                }
                
                // Apply window
                for (int i = 0; i < FRAME_SIZE; ++i) {
                    tempFrame[i] *= window[i];
                }
                
                // Overlap-add into accumulator
                for (int i = 0; i < FRAME_SIZE; ++i) {
                    olaBuffer[i] += tempFrame[i];
                }
                
                // Emit synthesisHop samples
                int toEmit = std::min(synthesisHop, outCapacity - outputSamples);
                for (int i = 0; i < toEmit; ++i) {
                    out[outputSamples + i] = olaBuffer[i];
                }
                outputSamples += toEmit;
                
                // Shift accumulator
                if (synthesisHop < FRAME_SIZE) {
                    std::memmove(olaBuffer, olaBuffer + synthesisHop, static_cast<size_t>(FRAME_SIZE - synthesisHop) * sizeof(float));
                    std::fill(olaBuffer + (FRAME_SIZE - synthesisHop), olaBuffer + FRAME_SIZE, 0.0f);
                } else {
                    std::fill(olaBuffer, olaBuffer + FRAME_SIZE, 0.0f);
                }
                
                // Advance input position
                basePos += ANALYSIS_HOP;
                
                // Discard processed input
                if (basePos > ANALYSIS_HOP) {
                    int toDiscard = basePos - ANALYSIS_HOP;
                    inputBuffer.discard(toDiscard);
                    basePos = ANALYSIS_HOP;
                }
                
                continue; // Try next frame
            }
            break; // Not enough input
        }
        
        // Get previous frame tail for correlation (from olaBuffer, not input buffer)
        // The prevTail should be the last OVERLAP samples from the previous synthesis frame
        // which are already in olaBuffer
        
        // Find best offset by correlating overlap regions
        float bestCorr = -1.0f;
        int bestOffset = 0;
        
        // Use prevTail from olaBuffer (last OVERLAP samples)
        // On first frame, prevTail might be zeros, which is OK
        float* prevTailData = olaBuffer + (FRAME_SIZE - OVERLAP);
        
        // Clamp seek range to valid offsets given current ring size
        int maxOffset = SEEK_RANGE;
        int minOffset = -SEEK_RANGE;
        
        // Ensure we don't search beyond available input
        int maxValidPos = inputBuffer.size() - OVERLAP;
        int minValidPos = 0;
        
        // Adjust search range based on available input
        if (basePos + ANALYSIS_HOP + maxOffset + OVERLAP > inputBuffer.size()) {
            maxOffset = inputBuffer.size() - (basePos + ANALYSIS_HOP + OVERLAP);
        }
        if (basePos + ANALYSIS_HOP + minOffset < 0) {
            minOffset = -(basePos + ANALYSIS_HOP);
        }
        
        // Ensure valid range
        if (minOffset > maxOffset || maxOffset < 0 || minOffset > 0) {
            TimePitchErrorStatus::getInstance().setError(TimePitchError::WSOLA_OOB);
            break; // Invalid search range
        }
        
        for (int offset = minOffset; offset <= maxOffset; ++offset) {
            int candidatePos = basePos + ANALYSIS_HOP + offset;
            
            // Double-check bounds
            if (candidatePos < 0 || candidatePos + OVERLAP > inputBuffer.size() || 
                candidatePos + FRAME_SIZE > inputBuffer.size()) {
                continue; // Skip invalid positions
            }
            
            // Get candidate overlap region from input buffer
            float candidateOverlap[OVERLAP];
            int peeked = inputBuffer.peek(candidateOverlap, OVERLAP, candidatePos);
            if (peeked < OVERLAP) {
                continue; // Not enough data
            }
            
            // Correlate with previous frame tail
            float corr = correlation(prevTailData, candidateOverlap, OVERLAP);
            
            // Check for NaN/Inf
            if (!std::isfinite(corr)) {
                corr = -1.0f; // Invalid correlation
            }
            
            if (corr > bestCorr) {
                bestCorr = corr;
                bestOffset = offset;
            }
        }
        
        // Copy chosen frame with bounds checking
        int chosenPos = basePos + ANALYSIS_HOP + bestOffset;
        if (chosenPos < 0 || chosenPos + FRAME_SIZE > inputBuffer.size()) {
            TimePitchErrorStatus::getInstance().setError(TimePitchError::WSOLA_OOB);
            break; // Invalid position
        }
        
        int peeked = inputBuffer.peek(tempFrame, FRAME_SIZE, chosenPos);
        if (peeked < FRAME_SIZE) {
            TimePitchErrorStatus::getInstance().setError(TimePitchError::WSOLA_UNDERFLOW);
            break; // Not enough data
        }
        
        // Apply window and check for NaN/Inf
        for (int i = 0; i < FRAME_SIZE; ++i) {
            tempFrame[i] *= window[i];
            if (!std::isfinite(tempFrame[i])) {
                tempFrame[i] = 0.0f;
            }
        }
        
        // Overlap-add into accumulator
        for (int i = 0; i < FRAME_SIZE; ++i) {
            olaBuffer[i] += tempFrame[i];
            if (!std::isfinite(olaBuffer[i])) {
                olaBuffer[i] = 0.0f;
            }
        }
        
        // Emit synthesisHop samples
        int toEmit = std::min(synthesisHop, outCapacity - outputSamples);
        for (int i = 0; i < toEmit; ++i) {
            float sample = olaBuffer[i];
            if (!std::isfinite(sample)) {
                sample = 0.0f;
            }
            out[outputSamples + i] = sample;
        }
        outputSamples += toEmit;
        
        // Shift accumulator left by synthesisHop
        if (synthesisHop < FRAME_SIZE) {
            std::memmove(olaBuffer, olaBuffer + synthesisHop, static_cast<size_t>(FRAME_SIZE - synthesisHop) * sizeof(float));
            std::fill(olaBuffer + (FRAME_SIZE - synthesisHop), olaBuffer + FRAME_SIZE, 0.0f);
        } else {
            std::fill(olaBuffer, olaBuffer + FRAME_SIZE, 0.0f);
        }
        
        // Advance input position
        basePos += ANALYSIS_HOP + bestOffset;
    }
    
    // Discard processed input (but keep some for next frame)
    // Only discard if we've processed enough
    if (basePos > ANALYSIS_HOP) {
        int toDiscard = basePos - ANALYSIS_HOP;
        inputBuffer.discard(toDiscard);
        basePos = ANALYSIS_HOP;
    }
    
    return outputSamples;
}

float WSOLA::correlation(const float* a, const float* b, int n)
{
    if (a == nullptr || b == nullptr || n <= 0) {
        return -1.0f;
    }
    
    // Normalized dot product (avoid div by 0)
    double dot = 0.0;
    double ea = 0.0;
    double eb = 0.0;
    
    for (int i = 0; i < n; ++i) {
        double x = static_cast<double>(a[i]);
        double y = static_cast<double>(b[i]);
        
        // Check for NaN/Inf
        if (!std::isfinite(x)) x = 0.0;
        if (!std::isfinite(y)) y = 0.0;
        
        dot += x * y;
        ea += x * x;
        eb += y * y;
    }
    
    // Ensure non-negative (should always be, but be safe)
    ea = std::max(0.0, ea);
    eb = std::max(0.0, eb);
    
    double denom = std::sqrt(ea * eb) + 1e-12;
    float result = static_cast<float>(dot / denom);
    
    // Check result for NaN/Inf
    if (!std::isfinite(result)) {
        result = -1.0f;
    }
    
    return result;
}

void WSOLA::allocateBuffers()
{
    deallocateBuffers();
    
    // Input buffer needs frameSize + seekRange + some extra
    inputBufferCapacity = FRAME_SIZE + SEEK_RANGE + ANALYSIS_HOP * 2;
    inputBufferStorage = new float[inputBufferCapacity];
    std::fill(inputBufferStorage, inputBufferStorage + inputBufferCapacity, 0.0f);
    inputBuffer.init(inputBufferStorage, inputBufferCapacity);
    
    olaBuffer = new float[FRAME_SIZE];
    prevTail = new float[OVERLAP];
    tempFrame = new float[FRAME_SIZE];
    window = new float[FRAME_SIZE];
    
    std::fill(olaBuffer, olaBuffer + FRAME_SIZE, 0.0f);
    std::fill(prevTail, prevTail + OVERLAP, 0.0f);
    std::fill(tempFrame, tempFrame + FRAME_SIZE, 0.0f);
    
    // Generate Hann window
    makeHann(window, FRAME_SIZE);
}

void WSOLA::deallocateBuffers()
{
    delete[] inputBufferStorage;
    delete[] olaBuffer;
    delete[] prevTail;
    delete[] tempFrame;
    delete[] window;
    
    inputBufferStorage = nullptr;
    olaBuffer = nullptr;
    prevTail = nullptr;
    tempFrame = nullptr;
    window = nullptr;
}

} // namespace Core

