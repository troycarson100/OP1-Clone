#pragma once

namespace Core {

/**
 * Portable interface for time-stretching and pitch-shifting
 * No JUCE dependencies - suitable for embedded hardware porting
 */
struct TimePitchConfig {
    int channels;
    double sampleRate;
    int maxBlockSize;
};

/**
 * Abstract interface for time-stretching and pitch-shifting processors
 * Implementations can use Signalsmith Stretch, custom algorithms, or hardware-specific code
 */
class ITimePitch {
public:
    virtual ~ITimePitch() = default;
    
    /**
     * Prepare the processor for audio processing
     * Must be called before process()
     * All buffers should be allocated here (no allocations in process())
     */
    virtual void prepare(const TimePitchConfig& config) = 0;
    
    /**
     * Reset internal state (e.g., on note-on)
     */
    virtual void reset() = 0;
    
    /**
     * Set pitch shift in semitones
     * Positive = higher pitch, negative = lower pitch
     */
    virtual void setPitchSemitones(float semitones) = 0;
    
    /**
     * Set time-stretch ratio
     * 1.0 = no time-stretch (original duration)
     * > 1.0 = stretch (longer duration)
     * < 1.0 = compress (shorter duration)
     */
    virtual void setTimeRatio(float ratio) = 0;
    
    /**
     * Process audio
     * @param in Input buffer (interleaved if channels > 1)
     * @param inN Number of input samples per channel
     * @param out Output buffer (interleaved if channels > 1)
     * @param outN Number of output samples per channel requested
     * @return Number of output samples per channel actually produced
     * 
     * Note: Time-stretching is controlled by the ratio of inN to outN over time.
     * For constant duration with pitch change: keep timeRatio = 1.0 and adjust pitch only.
     */
    virtual int process(const float* in, int inN, float* out, int outN) = 0;
    
    /**
     * Get input latency in samples (per channel)
     * Call after prepare()
     */
    virtual int getInputLatency() const = 0;
    
    /**
     * Get output latency in samples (per channel)
     * Call after prepare()
     */
    virtual int getOutputLatency() const = 0;
    
    /**
     * Flush remaining samples at end of playback
     * Call with silence input after sample ends to drain tail
     */
    virtual int flush(float* out, int outN) = 0;
    
    /**
     * Check if processor has been prepared
     * Returns true if prepare() has been called successfully
     */
    virtual bool isPrepared() const = 0;
};

} // namespace Core

