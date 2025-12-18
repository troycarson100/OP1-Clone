#include "SimpleFFT.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace Core {

SimpleFFT::SimpleFFT()
    : frameSize(0)
    , twiddleFactors(nullptr)
    , bitReverseTable(nullptr)
    , bitReverseIndices(nullptr)
{
}

SimpleFFT::~SimpleFFT()
{
    delete[] twiddleFactors;
    delete[] bitReverseTable;
    delete[] bitReverseIndices;
}

void SimpleFFT::prepare(int size)
{
    if (!isPowerOf2(size) || size < 2) {
        frameSize = 0;
        return; // Invalid size
    }
    
    frameSize = size;
    
    // Allocate twiddle factors (complex numbers)
    // We need frameSize/2 complex numbers = frameSize floats
    // Format: [real0, imag0, real1, imag1, ..., realN, imagN]
    delete[] twiddleFactors;
    twiddleFactors = new float[frameSize]; // frameSize/2 complex = frameSize floats
    std::fill(twiddleFactors, twiddleFactors + frameSize, 0.0f);
    
    // Allocate bit reverse table (for in-place FFT)
    delete[] bitReverseTable;
    bitReverseTable = new float[frameSize * 2]; // Complex
    
    delete[] bitReverseIndices;
    bitReverseIndices = new int[frameSize];
    
    computeTwiddleFactors();
    computeBitReverseTable();
}

void SimpleFFT::forward(const float* input, float* output)
{
    if (frameSize == 0 || input == nullptr || output == nullptr || 
        bitReverseIndices == nullptr || bitReverseTable == nullptr) {
        return;
    }
    
    // Copy input to working buffer with bit reversal
    for (int i = 0; i < frameSize; ++i) {
        int reversed = bitReverseIndices[i];
        if (reversed >= 0 && reversed < frameSize) {
            bitReverseTable[i * 2] = input[reversed];     // Real
            bitReverseTable[i * 2 + 1] = 0.0f;            // Imag
        } else {
            bitReverseTable[i * 2] = 0.0f;
            bitReverseTable[i * 2 + 1] = 0.0f;
        }
    }
    
    // Radix-2 FFT (in-place)
    int stages = log2(frameSize);
    int step = 1;
    
    for (int stage = 0; stage < stages; ++stage) {
        int jump = step * 2;
        
        for (int group = 0; group < step; ++group) {
            int twiddleIndex = group * (frameSize / jump);
            
            // Compute twiddle factor on-the-fly if index is out of precomputed range
            float wr, wi;
            if (twiddleIndex < frameSize / 2) {
                int twiddleIdx = twiddleIndex * 2;
                if (twiddleIdx + 1 < frameSize && twiddleFactors != nullptr) {
                    wr = twiddleFactors[twiddleIdx];
                    wi = twiddleFactors[twiddleIdx + 1];
                } else {
                    // Fallback: compute directly
                    const double pi = 3.14159265358979323846;
                    double angle = -2.0 * pi * twiddleIndex / frameSize;
                    wr = static_cast<float>(std::cos(angle));
                    wi = static_cast<float>(std::sin(angle));
                }
            } else {
                // Shouldn't happen, but compute directly as fallback
                const double pi = 3.14159265358979323846;
                double angle = -2.0 * pi * twiddleIndex / frameSize;
                wr = static_cast<float>(std::cos(angle));
                wi = static_cast<float>(std::sin(angle));
            }
            
            for (int pair = group; pair < frameSize; pair += jump) {
                int match = pair + step;
                
                float tr = wr * bitReverseTable[match * 2] - wi * bitReverseTable[match * 2 + 1];
                float ti = wr * bitReverseTable[match * 2 + 1] + wi * bitReverseTable[match * 2];
                
                bitReverseTable[match * 2] = bitReverseTable[pair * 2] - tr;
                bitReverseTable[match * 2 + 1] = bitReverseTable[pair * 2 + 1] - ti;
                
                bitReverseTable[pair * 2] += tr;
                bitReverseTable[pair * 2 + 1] += ti;
            }
        }
        
        step = jump;
    }
    
    // Copy to output (only first frameSize/2 + 1 bins for real FFT)
    int numBins = frameSize / 2 + 1;
    for (int i = 0; i < numBins; ++i) {
        output[i * 2] = bitReverseTable[i * 2];         // Real
        output[i * 2 + 1] = bitReverseTable[i * 2 + 1]; // Imag
    }
}

void SimpleFFT::inverse(const float* input, float* output)
{
    if (frameSize == 0 || input == nullptr || output == nullptr ||
        bitReverseIndices == nullptr || bitReverseTable == nullptr) {
        return;
    }
    
    // Copy input to working buffer (conjugate for IFFT)
    int numBins = frameSize / 2 + 1;
    for (int i = 0; i < numBins; ++i) {
        bitReverseTable[i * 2] = input[i * 2];         // Real (keep same)
        bitReverseTable[i * 2 + 1] = -input[i * 2 + 1]; // Imag (conjugate)
    }
    
    // Fill in negative frequencies (Hermitian symmetry for real signal)
    for (int i = numBins; i < frameSize; ++i) {
        int mirror = frameSize - i;
        bitReverseTable[i * 2] = bitReverseTable[mirror * 2];         // Real
        bitReverseTable[i * 2 + 1] = -bitReverseTable[mirror * 2 + 1]; // Imag (conjugate)
    }
    
    // Apply bit reversal
    float* temp = new float[frameSize * 2];
    for (int i = 0; i < frameSize; ++i) {
        int reversed = bitReverseIndices[i];
        temp[i * 2] = bitReverseTable[reversed * 2];
        temp[i * 2 + 1] = bitReverseTable[reversed * 2 + 1];
    }
    std::memcpy(bitReverseTable, temp, frameSize * 2 * sizeof(float));
    delete[] temp;
    
    // Radix-2 IFFT (same as FFT but with conjugated twiddles)
    int stages = log2(frameSize);
    int step = 1;
    
    for (int stage = 0; stage < stages; ++stage) {
        int jump = step * 2;
        
        for (int group = 0; group < step; ++group) {
            int twiddleIndex = group * (frameSize / jump);
            
            // Compute twiddle factor on-the-fly if index is out of precomputed range
            float wr, wi;
            if (twiddleIndex < frameSize / 2) {
                int twiddleIdx = twiddleIndex * 2;
                if (twiddleIdx + 1 < frameSize && twiddleFactors != nullptr) {
                    wr = twiddleFactors[twiddleIdx];
                    wi = -twiddleFactors[twiddleIdx + 1]; // Conjugate for IFFT
                } else {
                    // Fallback: compute directly
                    const double pi = 3.14159265358979323846;
                    double angle = 2.0 * pi * twiddleIndex / frameSize; // Positive for IFFT
                    wr = static_cast<float>(std::cos(angle));
                    wi = static_cast<float>(std::sin(angle));
                }
            } else {
                // Shouldn't happen, but compute directly as fallback
                const double pi = 3.14159265358979323846;
                double angle = 2.0 * pi * twiddleIndex / frameSize; // Positive for IFFT
                wr = static_cast<float>(std::cos(angle));
                wi = static_cast<float>(std::sin(angle));
            }
            
            for (int pair = group; pair < frameSize; pair += jump) {
                int match = pair + step;
                
                float tr = wr * bitReverseTable[match * 2] - wi * bitReverseTable[match * 2 + 1];
                float ti = wr * bitReverseTable[match * 2 + 1] + wi * bitReverseTable[match * 2];
                
                bitReverseTable[match * 2] = bitReverseTable[pair * 2] - tr;
                bitReverseTable[match * 2 + 1] = bitReverseTable[pair * 2 + 1] - ti;
                
                bitReverseTable[pair * 2] += tr;
                bitReverseTable[pair * 2 + 1] += ti;
            }
        }
        
        step = jump;
    }
    
    // Copy to output and normalize
    float norm = 1.0f / static_cast<float>(frameSize);
    for (int i = 0; i < frameSize; ++i) {
        output[i] = bitReverseTable[i * 2] * norm;
    }
}

bool SimpleFFT::isPowerOf2(int n) const
{
    return (n > 0) && ((n & (n - 1)) == 0);
}

int SimpleFFT::log2(int n) const
{
    int result = 0;
    while (n > 1) {
        n >>= 1;
        result++;
    }
    return result;
}

void SimpleFFT::computeTwiddleFactors()
{
    const double pi = 3.14159265358979323846;
    int numTwiddles = frameSize / 2;
    
    // Safety check
    if (twiddleFactors == nullptr || numTwiddles == 0 || frameSize < 2) {
        return;
    }
    
    // Compute twiddle factors for all needed indices
    // For radix-2 FFT, we need twiddles for indices: 0, frameSize/2, frameSize/4, frameSize/8, ...
    // But we'll compute them on-demand in the FFT loop
    // For now, pre-compute the first numTwiddles twiddles
    for (int i = 0; i < numTwiddles; ++i) {
        double angle = -2.0 * pi * i / frameSize;
        int idx = i * 2;
        if (idx + 1 < frameSize) {
            twiddleFactors[idx] = static_cast<float>(std::cos(angle));
            twiddleFactors[idx + 1] = static_cast<float>(std::sin(angle));
        }
    }
}

void SimpleFFT::computeBitReverseTable()
{
    int bits = log2(frameSize);
    
    for (int i = 0; i < frameSize; ++i) {
        int reversed = 0;
        int temp = i;
        for (int j = 0; j < bits; ++j) {
            reversed = (reversed << 1) | (temp & 1);
            temp >>= 1;
        }
        bitReverseIndices[i] = reversed;
    }
}

} // namespace Core

