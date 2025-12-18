#include "STFT.h"
#include <algorithm>
#include <cstring>

namespace Core {

STFT::STFT()
    : frameSize(0)
    , hopSize(0)
    , sampleRate(44100.0)
    , window(nullptr)
    , analysisBuffer(nullptr)
    , synthesisBuffer(nullptr)
    , complexSpectrum(nullptr)
{
}

STFT::~STFT()
{
    deallocateBuffers();
}

void STFT::prepare(int size, int hop, double rate)
{
    frameSize = size;
    hopSize = hop;
    sampleRate = rate;
    
    fft.prepare(frameSize);
    allocateBuffers();
    
    // Generate normalized Hann window for perfect reconstruction
    WindowFunctions::generateHannNormalized(window, frameSize);
}

void STFT::analyze(const float* input, float* output)
{
    if (input == nullptr || output == nullptr || frameSize == 0) {
        return;
    }
    
    // Apply window
    for (int i = 0; i < frameSize; ++i) {
        analysisBuffer[i] = input[i] * window[i];
    }
    
    // Forward FFT
    fft.forward(analysisBuffer, output);
}

void STFT::synthesize(const float* input, float* output)
{
    if (input == nullptr || output == nullptr || frameSize == 0) {
        return;
    }
    
    // Inverse FFT
    fft.inverse(input, synthesisBuffer);
    
    // Apply window and copy to output (for overlap-add)
    for (int i = 0; i < frameSize; ++i) {
        output[i] = synthesisBuffer[i] * window[i];
    }
}

void STFT::allocateBuffers()
{
    deallocateBuffers();
    
    window = new float[frameSize];
    analysisBuffer = new float[frameSize];
    synthesisBuffer = new float[frameSize];
    complexSpectrum = new float[frameSize + 2]; // Complex: frameSize/2 + 1 bins
    
    std::fill(window, window + frameSize, 0.0f);
    std::fill(analysisBuffer, analysisBuffer + frameSize, 0.0f);
    std::fill(synthesisBuffer, synthesisBuffer + frameSize, 0.0f);
    std::fill(complexSpectrum, complexSpectrum + frameSize + 2, 0.0f);
}

void STFT::deallocateBuffers()
{
    delete[] window;
    delete[] analysisBuffer;
    delete[] synthesisBuffer;
    delete[] complexSpectrum;
    
    window = nullptr;
    analysisBuffer = nullptr;
    synthesisBuffer = nullptr;
    complexSpectrum = nullptr;
}

} // namespace Core

