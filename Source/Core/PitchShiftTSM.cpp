#include "PitchShiftTSM.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace Core {

PitchShiftTSM::PitchShiftTSM()
    : frameSize(DEFAULT_FRAME_SIZE)
    , hopSize(DEFAULT_HOP_SIZE)
    , sampleRate(44100.0)
    , lastPhase(nullptr)
    , currentPhase(nullptr)
    , magnitude(nullptr)
    , complexSpectrum(nullptr)
    , pitchRatio(1.0f)
    , numBins(0)
    , readHead(0.0)
    , lastMagnitude(nullptr)
    , spectralFluxThreshold(0.1f)
{
}

PitchShiftTSM::~PitchShiftTSM()
{
    deallocateBuffers();
}

void PitchShiftTSM::prepare(double rate, int maxBlockSize)
{
    sampleRate = rate;
    
    // Use default frame size (1024) and hop (256)
    frameSize = DEFAULT_FRAME_SIZE;
    hopSize = DEFAULT_HOP_SIZE;
    
    // Prepare STFT
    stft.prepare(frameSize, hopSize, sampleRate);
    
    // Prepare circular buffers (need enough space for frame + some extra)
    int bufferSize = frameSize * 4; // Enough for multiple frames
    inputBuffer.prepare(bufferSize);
    outputBuffer.prepare(bufferSize);
    
    numBins = frameSize / 2 + 1;
    allocateBuffers();
    
    reset();
}

void PitchShiftTSM::setPitchRatio(float ratio)
{
    // Clamp to reasonable range
    pitchRatio = std::max(0.25f, std::min(4.0f, ratio));
}

int PitchShiftTSM::process(const float* input, float* output, int numSamples)
{
    if (input == nullptr || output == nullptr || numSamples <= 0) {
        return 0;
    }
    
    // TEMPORARY: Use simple resampling until FFT is fixed
    // This provides pitch shifting without time-stretching, but at least it works
    // For time-stretching, we need the phase vocoder which requires working FFT
    
    // Simple approach: if pitchRatio == 1.0, just pass through
    // Otherwise, resample the input block
    if (std::abs(pitchRatio - 1.0f) < 0.001f) {
        // No pitch shift - pass through
        std::memcpy(output, input, numSamples * sizeof(float));
        return numSamples;
    }
    
    // Simple pitch shifting via resampling (no time-stretching)
    // Read from input at a rate determined by pitchRatio
    for (int i = 0; i < numSamples; ++i) {
        int idx0 = static_cast<int>(readHead);
        int idx1 = idx0 + 1;
        float fraction = static_cast<float>(readHead - static_cast<double>(idx0));
        
        if (idx0 >= numSamples) {
            output[i] = 0.0f;
        } else if (idx1 >= numSamples) {
            output[i] = input[idx0];
        } else {
            output[i] = input[idx0] * (1.0f - fraction) + input[idx1] * fraction;
        }
        
        // Advance read head at pitch ratio speed
        // For pitch up (ratio > 1), read faster through input
        // For pitch down (ratio < 1), read slower
        readHead += pitchRatio;
        
        // Wrap or stop when we've read all input
        if (readHead >= numSamples) {
            readHead = 0.0; // Loop for now
        }
    }
    
    return numSamples;
    
    /* ORIGINAL PHASE VOCODER CODE - DISABLED UNTIL FFT IS FIXED
    // Safety check: ensure pitch shifter is prepared
    if (numBins == 0 || lastPhase == nullptr || complexSpectrum == nullptr) {
        // Not prepared - return zeros to prevent crashes
        std::fill(output, output + numSamples, 0.0f);
        return numSamples;
    }
    
    // Zero output buffer first (for overlap-add)
    std::fill(output, output + numSamples, 0.0f);
    
    // Write input to circular buffer (accumulates across calls)
    inputBuffer.write(input, numSamples);
    
    // Process frames until we have enough output or run out of input
    int outputSamples = 0;
    int maxIterations = 10; // Prevent infinite loops
    int iterations = 0;
    
    while (inputBuffer.getNumAvailable() >= frameSize && 
           outputSamples < numSamples &&
           iterations < maxIterations) {
        processFrame();
        iterations++;
        
        // Read output frame (overlap-add result)
        float frameOutput[1024]; // Max frame size
        int frameSamples = outputBuffer.read(frameOutput, frameSize);
        
        // Copy to output with overlap-add
        int toCopy = std::min(frameSamples, numSamples - outputSamples);
        for (int i = 0; i < toCopy; ++i) {
            output[outputSamples + i] += frameOutput[i];
        }
        
        outputSamples += toCopy;
    }
    
    // If we didn't produce enough output but have some input, 
    // we need to wait for more input (latency). Return what we have.
    // The circular buffer will accumulate more input on next call.
    
    // Always return at least numSamples (zero-padded if needed)
    // This ensures notes always trigger, even with latency
    if (outputSamples < numSamples) {
        // Pad with zeros - this is expected during initial latency period
        std::fill(output + outputSamples, output + numSamples, 0.0f);
        return numSamples;
    }
    
    return outputSamples;
    */
}

void PitchShiftTSM::processFrame()
{
    // DISABLED - using simple resampling instead until FFT is fixed
    // Phase vocoder code is commented out - see original implementation below
    return;
    
    /* ORIGINAL PHASE VOCODER CODE - DISABLED
    // Safety check
    if (numBins == 0 || lastPhase == nullptr || currentPhase == nullptr || 
        magnitude == nullptr || complexSpectrum == nullptr) {
        return;
    }
    
    // Read frame from input buffer
    float frameInput[1024];
    int read = inputBuffer.peek(frameInput, frameSize);
    if (read < frameSize) {
        return; // Not enough data
    }
    
    // Analyze frame
    stft.analyze(frameInput, complexSpectrum);
    
    // Extract magnitude and phase
    const double pi = 3.14159265358979323846;
    const double twoPi = 2.0 * pi;
    const double phaseIncrement = twoPi * hopSize / frameSize;
    
    for (int i = 0; i < numBins; ++i) {
        float real = complexSpectrum[i * 2];
        float imag = complexSpectrum[i * 2 + 1];
        
        magnitude[i] = std::sqrt(real * real + imag * imag);
        currentPhase[i] = std::atan2(imag, real);
    }
    
    // Phase unwrapping and pitch shifting
    // This is the core of the phase vocoder algorithm
    for (int i = 0; i < numBins; ++i) {
        // Calculate expected phase advance for this bin
        double expectedPhaseAdvance = phaseIncrement * i;
        
        // Calculate actual phase difference
        float phaseDiff = currentPhase[i] - lastPhase[i];
        
        // Unwrap phase (handle 2π wraparound)
        while (phaseDiff > pi) phaseDiff -= twoPi;
        while (phaseDiff < -pi) phaseDiff += twoPi;
        
        // Add expected advance to get unwrapped phase
        float unwrappedPhase = lastPhase[i] + phaseDiff + static_cast<float>(expectedPhaseAdvance);
        
        // Scale phase advance by pitch ratio (this changes pitch)
        // For pitch up: phase advances faster, for pitch down: slower
        float scaledPhase = lastPhase[i] + phaseDiff * pitchRatio + static_cast<float>(expectedPhaseAdvance * pitchRatio);
        
        // Store for next frame
        lastPhase[i] = scaledPhase;
        
        // Reconstruct complex spectrum with new phase
        complexSpectrum[i * 2] = magnitude[i] * std::cos(scaledPhase);     // Real
        complexSpectrum[i * 2 + 1] = magnitude[i] * std::sin(scaledPhase); // Imag
    }
    
    // Simple transient detection (spectral flux)
    float flux = detectTransient(magnitude);
    bool isTransient = (flux > spectralFluxThreshold);
    
    // For transients, reduce phase propagation to avoid phasiness
    if (isTransient) {
        // Use more of the current phase, less of the propagated phase
        for (int i = 1; i < numBins - 1; ++i) {
            float real = complexSpectrum[i * 2];
            float imag = complexSpectrum[i * 2 + 1];
            float mag = std::sqrt(real * real + imag * imag);
            float phase = std::atan2(imag, real);
            
            // Blend between current and propagated phase
            float blendedPhase = 0.7f * phase + 0.3f * lastPhase[i];
            
            complexSpectrum[i * 2] = mag * std::cos(blendedPhase);
            complexSpectrum[i * 2 + 1] = mag * std::sin(blendedPhase);
        }
    }
    
    // Synthesize frame
    float frameOutput[1024];
    stft.synthesize(complexSpectrum, frameOutput);
    
    // Write to output buffer (overlap-add will happen in process())
    outputBuffer.write(frameOutput, frameSize);
    
    // Advance input buffer by hop size
    float dummy[256];
    inputBuffer.read(dummy, hopSize);
    
    // Update last magnitude for transient detection
    std::memcpy(lastMagnitude, magnitude, numBins * sizeof(float));
    */
}

void PitchShiftTSM::unwrapPhase(float* phase, const float* lastPhase, int numBins)
{
    const double pi = 3.14159265358979323846;
    const double twoPi = 2.0 * pi;
    
    for (int i = 0; i < numBins; ++i) {
        float diff = phase[i] - lastPhase[i];
        
        // Unwrap: add/subtract 2π to bring diff into [-π, π]
        while (diff > pi) diff -= twoPi;
        while (diff < -pi) diff += twoPi;
        
        phase[i] = lastPhase[i] + diff;
    }
}

float PitchShiftTSM::detectTransient(const float* magnitude)
{
    if (lastMagnitude == nullptr) {
        return 0.0f;
    }
    
    float flux = 0.0f;
    for (int i = 1; i < numBins; ++i) {
        float diff = magnitude[i] - lastMagnitude[i];
        if (diff > 0.0f) {
            flux += diff;
        }
    }
    
    // Normalize by number of bins
    return flux / static_cast<float>(numBins);
}

void PitchShiftTSM::reset()
{
    inputBuffer.clear();
    outputBuffer.clear();
    
    // Reset simple resampling read head
    readHead = 0.0;
    
    if (lastPhase != nullptr) {
        std::fill(lastPhase, lastPhase + numBins, 0.0f);
        std::fill(currentPhase, currentPhase + numBins, 0.0f);
        std::fill(magnitude, magnitude + numBins, 0.0f);
    }
    
    if (lastMagnitude != nullptr) {
        std::fill(lastMagnitude, lastMagnitude + numBins, 0.0f);
    }
}

void PitchShiftTSM::allocateBuffers()
{
    deallocateBuffers();
    
    numBins = frameSize / 2 + 1;
    
    lastPhase = new float[numBins];
    currentPhase = new float[numBins];
    magnitude = new float[numBins];
    complexSpectrum = new float[frameSize + 2]; // Complex spectrum
    lastMagnitude = new float[numBins];
    
    std::fill(lastPhase, lastPhase + numBins, 0.0f);
    std::fill(currentPhase, currentPhase + numBins, 0.0f);
    std::fill(magnitude, magnitude + numBins, 0.0f);
    std::fill(complexSpectrum, complexSpectrum + frameSize + 2, 0.0f);
    std::fill(lastMagnitude, lastMagnitude + numBins, 0.0f);
}

void PitchShiftTSM::deallocateBuffers()
{
    delete[] lastPhase;
    delete[] currentPhase;
    delete[] magnitude;
    delete[] complexSpectrum;
    delete[] lastMagnitude;
    
    lastPhase = nullptr;
    currentPhase = nullptr;
    magnitude = nullptr;
    complexSpectrum = nullptr;
    lastMagnitude = nullptr;
}

} // namespace Core

