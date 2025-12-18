#include "CircularBuffer.h"
#include <algorithm>
#include <cstring>

namespace Core {

CircularBuffer::CircularBuffer()
    : buffer(nullptr)
    , bufferSize(0)
    , writePos(0)
    , readPos(0)
    , numAvailable(0)
{
}

CircularBuffer::~CircularBuffer()
{
    delete[] buffer;
}

void CircularBuffer::prepare(int size)
{
    if (buffer != nullptr) {
        delete[] buffer;
    }
    
    bufferSize = size;
    buffer = new float[bufferSize];
    std::fill(buffer, buffer + bufferSize, 0.0f);
    writePos = 0;
    readPos = 0;
    numAvailable = 0;
}

void CircularBuffer::write(const float* data, int numSamples)
{
    if (buffer == nullptr || numSamples <= 0) {
        return;
    }
    
    for (int i = 0; i < numSamples; ++i) {
        buffer[writePos] = data[i];
        writePos = wrapIndex(writePos + 1);
        
        if (numAvailable < bufferSize) {
            numAvailable++;
        } else {
            // Buffer full, advance read pointer (overwrite oldest)
            readPos = wrapIndex(readPos + 1);
        }
    }
}

int CircularBuffer::peek(float* output, int numSamples, int offset) const
{
    if (buffer == nullptr || output == nullptr || numSamples <= 0) {
        return 0;
    }
    
    int available = numAvailable - offset;
    if (available <= 0) {
        return 0;
    }
    
    int toRead = (numSamples < available) ? numSamples : available;
    int currentReadPos = wrapIndex(readPos + offset);
    
    for (int i = 0; i < toRead; ++i) {
        output[i] = buffer[currentReadPos];
        currentReadPos = wrapIndex(currentReadPos + 1);
    }
    
    // Zero out remaining if requested more than available
    for (int i = toRead; i < numSamples; ++i) {
        output[i] = 0.0f;
    }
    
    return toRead;
}

int CircularBuffer::read(float* output, int numSamples)
{
    int read = peek(output, numSamples, 0);
    
    if (read > 0) {
        readPos = wrapIndex(readPos + read);
        numAvailable -= read;
    }
    
    return read;
}

int CircularBuffer::getNumAvailable() const
{
    return numAvailable;
}

void CircularBuffer::clear()
{
    if (buffer != nullptr) {
        std::fill(buffer, buffer + bufferSize, 0.0f);
    }
    writePos = 0;
    readPos = 0;
    numAvailable = 0;
}

int CircularBuffer::wrapIndex(int index) const
{
    if (bufferSize == 0) {
        return 0;
    }
    
    while (index < 0) {
        index += bufferSize;
    }
    while (index >= bufferSize) {
        index -= bufferSize;
    }
    return index;
}

} // namespace Core

