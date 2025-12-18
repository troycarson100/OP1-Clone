#include "RingBufferF.h"

namespace Core {

RingBufferF::RingBufferF()
    : buf_(nullptr)
    , cap_(0)
    , r_(0)
    , w_(0)
    , size_(0)
{
}

void RingBufferF::init(float* storage, int capacity)
{
    buf_ = storage;
    cap_ = capacity;
    r_ = 0;
    w_ = 0;
    size_ = 0;
}

int RingBufferF::push(const float* in, int n)
{
    if (buf_ == nullptr || in == nullptr || n <= 0) {
        return 0;
    }
    
    int can = std::min(n, freeSpace());
    for (int i = 0; i < can; ++i) {
        buf_[w_] = in[i];
        w_ = (w_ + 1) % cap_;
    }
    size_ += can;
    return can;
}

int RingBufferF::peek(float* out, int n, int offset) const
{
    if (buf_ == nullptr || out == nullptr || n <= 0 || offset < 0) {
        return 0;
    }
    
    int can = std::min(n, size_ - offset);
    if (can <= 0) {
        return 0;
    }
    
    int idx = (r_ + offset) % cap_;
    for (int i = 0; i < can; ++i) {
        out[i] = buf_[idx];
        idx = (idx + 1) % cap_;
    }
    
    // Zero remaining if requested more than available
    for (int i = can; i < n; ++i) {
        out[i] = 0.0f;
    }
    
    return can;
}

int RingBufferF::pop(float* out, int n)
{
    if (buf_ == nullptr || n <= 0) {
        return 0;
    }
    
    int can = std::min(n, size_);
    for (int i = 0; i < can; ++i) {
        if (out != nullptr) {
            out[i] = buf_[r_];
        }
        r_ = (r_ + 1) % cap_;
    }
    size_ -= can;
    return can;
}

void RingBufferF::discard(int n)
{
    int can = std::min(n, size_);
    r_ = (r_ + can) % cap_;
    size_ -= can;
}

void RingBufferF::reset()
{
    r_ = 0;
    w_ = 0;
    size_ = 0;
}

} // namespace Core

