#include "MoogLadderFilter.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <memory>

namespace Core {

// Pimpl implementation struct
struct MoogLadderFilterImpl {
    juce::dsp::LadderFilter<float> filter;
    juce::dsp::ProcessSpec spec;
    float tempInputBuffer[1];
    float tempOutputBuffer[1];
    
    MoogLadderFilterImpl() {
        // Initialize with default mode (LPF24 for 24dB/octave)
        filter.setMode(juce::dsp::LadderFilterMode::LPF24);
        filter.setEnabled(true);
    }
};

MoogLadderFilter::MoogLadderFilter()
    : pimpl(std::make_unique<MoogLadderFilterImpl>())
{
}

MoogLadderFilter::~MoogLadderFilter()
{
    // pimpl will be automatically destroyed
}

void MoogLadderFilter::prepare(double rate)
{
    pimpl->spec.sampleRate = rate;
    pimpl->spec.maximumBlockSize = 512;  // Reasonable default, can handle single samples
    pimpl->spec.numChannels = 1;  // Mono processing
    
    pimpl->filter.prepare(pimpl->spec);
    pimpl->filter.reset();
    
    // Set initial parameters
    if (pimpl->spec.sampleRate > 0.0) {
        pimpl->filter.setCutoffFrequencyHz(20000.0f);  // Start fully open
        pimpl->filter.setResonance(0.0f);
        // Don't set drive here - it will be set via setDrive() when needed
        // Default JUCE drive is 1.2, but we want 1.0 (clean) as default
        pimpl->filter.setDrive(1.0f);  // Clean, no drive initially
    }
}

void MoogLadderFilter::setCutoff(float cutoff)
{
    const float clampedCutoff = std::max(20.0f, std::min(20000.0f, cutoff));
    pimpl->filter.setCutoffFrequencyHz(clampedCutoff);
}

void MoogLadderFilter::setCutoffNoUpdate(float cutoff)
{
    // JUCE's filter handles smoothing internally, so we can just set it
    // The filter will smooth the change automatically
    setCutoff(cutoff);
}

void MoogLadderFilter::setResonance(float res)
{
    // Map from 0-4 range to 0-1 range for JUCE
    // JUCE's resonance is 0-1, where 1.0 can self-oscillate
    // Our range is 0-4, so we map: 0->0, 4->1
    const float clampedRes = std::max(0.0f, std::min(4.0f, res));
    const float juceResonance = clampedRes / 4.0f;  // Map 0-4 to 0-1
    pimpl->filter.setResonance(juceResonance);
}

void MoogLadderFilter::setDrive(float drv)
{
    // JUCE's drive is >= 1.0, where 1.0 = minimal/no drive, higher = more drive
    // Our drive is 0.0-1.0+, where 0.0 = no drive
    // Map EXTREMELY aggressively: 0.0 -> 1.0 (clean), 1.0 -> 10.0+ (very strong drive)
    // JUCE's drive uses tanh saturation: tanh(drive * input), so higher drive = more saturation
    // At drive=1.0, tanh(1.0*x) is subtle. At drive=10.0, tanh(10.0*x) saturates much more
    const float clampedDrive = std::max(0.0f, drv);
    float juceDrive;
    if (clampedDrive <= 1.0f) {
        // Map 0.0-1.0 to 1.0-10.0 for EXTREMELY strong, noticeable drive effect
        // 0.0 -> 1.0 (no drive), 1.0 -> 10.0 (maximum drive)
        juceDrive = 1.0f + (clampedDrive * 9.0f);  // 0.0->1.0, 1.0->10.0
    } else {
        // For values > 1.0, scale even more aggressively
        juceDrive = 10.0f + ((clampedDrive - 1.0f) * 5.0f);
    }
    pimpl->filter.setDrive(juceDrive);
}

float MoogLadderFilter::process(float input)
{
    // JUCE's filter expects AudioBlock, so we need to wrap single sample
    pimpl->tempInputBuffer[0] = input;
    pimpl->tempOutputBuffer[0] = input;  // Copy input to output first
    
    // Create channel pointer arrays for AudioBlock
    float* inputChannels[1] = { pimpl->tempInputBuffer };
    float* outputChannels[1] = { pimpl->tempOutputBuffer };
    
    // Create AudioBlocks for input and output
    juce::dsp::AudioBlock<const float> inputBlock(const_cast<const float**>(inputChannels), 1, 1);
    juce::dsp::AudioBlock<float> outputBlock(outputChannels, 1, 1);
    
    // Copy input to output
    outputBlock.copyFrom(inputBlock);
    
    // Create process context
    juce::dsp::ProcessContextReplacing<float> context(outputBlock);
    
    // Process through JUCE filter
    pimpl->filter.process(context);
    
    return pimpl->tempOutputBuffer[0];
}

void MoogLadderFilter::processBlock(const float* input, float* output, int numSamples)
{
    if (input == nullptr || output == nullptr || numSamples <= 0) {
        return;
    }
    
    // Create channel pointer arrays for AudioBlock
    // AudioBlock expects an array of channel pointers
    const float* inputChannels[1] = { input };
    float* outputChannels[1] = { output };
    
    // Create AudioBlocks pointing to the input/output buffers
    // Note: AudioBlock doesn't own the memory, it just references it
    juce::dsp::AudioBlock<const float> inputBlock(inputChannels, 1, static_cast<size_t>(numSamples));
    juce::dsp::AudioBlock<float> outputBlock(outputChannels, 1, static_cast<size_t>(numSamples));
    
    // Copy input to output first (JUCE will process in-place)
    outputBlock.copyFrom(inputBlock);
    
    // Create process context
    juce::dsp::ProcessContextReplacing<float> context(outputBlock);
    
    // Process through JUCE filter
    pimpl->filter.process(context);
}

void MoogLadderFilter::reset()
{
    pimpl->filter.reset();
}

} // namespace Core
