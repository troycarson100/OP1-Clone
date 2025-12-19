#pragma once

#include <vector>
#include <memory>

namespace Core {

// Immutable sample data structure
// Once created, the data never changes, ensuring thread safety
struct SampleData {
    std::vector<float> mono;
    int length = 0;
    double sourceSampleRate = 44100.0;
};

using SampleDataPtr = std::shared_ptr<const SampleData>;

} // namespace Core

