#include "EnvelopeGenerator.h"
#include <algorithm>
#include <cmath>

namespace Core {

EnvelopeGenerator::EnvelopeGenerator()
    : sampleRate(44100.0)
    , attackMs(10.0f)
    , releaseMs(100.0f)
    , state(State::Idle)
    , currentValue(0.0f)
    , attackSamples(1)
    , releaseSamples(1)
    , phaseCounter(0)
{
}

EnvelopeGenerator::~EnvelopeGenerator()
{
}

void EnvelopeGenerator::prepare(double rate)
{
    sampleRate = rate;
    updatePhaseCounts();
    reset();
}

void EnvelopeGenerator::setAttack(float attack)
{
    attackMs = std::max(0.0f, attack);
    updatePhaseCounts();
}

void EnvelopeGenerator::setRelease(float release)
{
    releaseMs = std::max(0.0f, release);
    updatePhaseCounts();
}

void EnvelopeGenerator::updatePhaseCounts()
{
    // Safety check: ensure sample rate is valid
    if (sampleRate <= 0.0) {
        attackSamples = 1;
        releaseSamples = 1;
        return;
    }
    
    attackSamples = static_cast<int>(sampleRate * attackMs / 1000.0);
    if (attackSamples < 1) attackSamples = 1;
    
    releaseSamples = static_cast<int>(sampleRate * releaseMs / 1000.0);
    if (releaseSamples < 1) releaseSamples = 1;
}

float EnvelopeGenerator::calculateAttackIncrement() const
{
    if (attackSamples <= 0) return 1.0f;
    return 1.0f / static_cast<float>(attackSamples);
}

float EnvelopeGenerator::calculateReleaseDecrement() const
{
    if (releaseSamples <= 0) return 1.0f;
    return 1.0f / static_cast<float>(releaseSamples);
}

void EnvelopeGenerator::trigger()
{
    state = State::Attack;
    phaseCounter = 0;
    // Start from current value (for retriggering)
}

void EnvelopeGenerator::release()
{
    if (state != State::Idle) {
        state = State::Release;
        phaseCounter = 0;
    }
}

float EnvelopeGenerator::process()
{
    switch (state) {
        case State::Idle:
            currentValue = 0.0f;
            break;
            
        case State::Attack:
            currentValue += calculateAttackIncrement();
            phaseCounter++;
            
            if (currentValue >= 1.0f || phaseCounter >= attackSamples) {
                currentValue = 1.0f;
                state = State::Idle; // Hold at 1.0 until released
            }
            break;
            
        case State::Release:
            currentValue -= calculateReleaseDecrement();
            phaseCounter++;
            
            if (currentValue <= 0.0f || phaseCounter >= releaseSamples) {
                currentValue = 0.0f;
                state = State::Idle;
            }
            break;
    }
    
    // Clamp value
    currentValue = std::max(0.0f, std::min(1.0f, currentValue));
    
    return currentValue;
}

void EnvelopeGenerator::processBlock(float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i) {
        output[i] = process();
    }
}

void EnvelopeGenerator::reset()
{
    state = State::Idle;
    currentValue = 0.0f;
    phaseCounter = 0;
}

} // namespace Core

