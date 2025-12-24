#pragma once

namespace Core {

/**
 * Interface for time-stretching/pitch-shifting processors
 * Portable C++ - no JUCE dependencies
 */
class IWarpProcessor {
public:
    virtual ~IWarpProcessor() = default;
    
    /**
     * Prepare processor for audio processing
     * @param sampleRate Sample rate in Hz
     * @param channels Number of audio channels (1 = mono, 2 = stereo)
     * @param maxBlockFrames Maximum block size in frames
     */
    virtual void prepare(double sampleRate, int channels, int maxBlockFrames) = 0;
    
    /**
     * Reset processor state (call on voice activation)
     */
    virtual void reset() = 0;
    
    /**
     * Set time ratio (duration multiplier)
     * @param ratio 1.0 = constant duration (pitch-only), != 1.0 = time stretching
     *              0.5 = slow down 2x, 2.0 = speed up 2x
     */
    virtual void setTimeRatio(double ratio) = 0;
    
    /**
     * Set pitch in semitones
     * @param semitones Pitch offset in semitones (-24 to +24)
     */
    virtual void setPitchSemitones(float semitones) = 0;
    
    /**
     * Process audio
     * @param in Planar input buffers [channels][frames]
     * @param inFrames Number of input frames
     * @param out Planar output buffers [channels][frames]
     * @param outFrames Number of output frames requested
     * @return Number of output frames actually produced
     */
    virtual int process(const float* const* in, int inFrames,
                       float* const* out, int outFrames) = 0;
    
    /**
     * Get output latency in frames
     * @return Latency in frames
     */
    virtual int getLatencyFrames() const = 0;
    
    /**
     * Check if processor is prepared
     */
    virtual bool isPrepared() const = 0;
    
    /**
     * Flush remaining output (call on sample end/noteOff)
     * @param out Planar output buffers [channels][frames]
     * @param outFrames Maximum frames to flush
     * @return Number of frames flushed
     */
    virtual int flush(float* const* out, int outFrames) = 0;
    
    /**
     * Get current output ring buffer fill level (for debugging)
     */
    virtual int getOutputRingFill() const = 0;
};

} // namespace Core


