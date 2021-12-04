#include "looper.h"
#include "dsp.h"

using namespace wreath;
using namespace daisysp;

/**
 * @brief Initializes the looper.
 *
 * @param sampleRate
 * @param mem
 * @param maxBufferSeconds
 */
void Looper::Init(size_t sampleRate, float *mem, int maxBufferSeconds)
{
    sampleRate_ = sampleRate;
    buffer_ = mem;
    initBufferSamples_ = sampleRate * maxBufferSeconds;
    ResetBuffer();

    state_ = State::INIT;
    mode_ = Mode::MIMEO;
    movement_ = Movement::FORWARD;
    forward_ = Movement::FORWARD == movement_;

    cf.Init(CROSSFADE_CPOW);
}

/**
 * @brief Sets the speed, clamping its value just in case.
 *
 * @param speed
 */
void Looper::SetSpeed(float speed)
{
    speed_ = fclamp(speed, 0.f, 2.f);
}

/**
 * @brief Resets the buffer and the looper state.
 */
void Looper::ResetBuffer()
{
    std::fill(&buffer_[0], &buffer_[initBufferSamples_ - 1], 0);
    bufferSamples_ = 0;
    bufferSeconds_ = 0;
    state_ = State::BUFFERING;
    feedback_ = 0;
    feedbackPickup_ = false;
    readPos_ = 0;
    readPosSeconds_ = 0;
    writePos_ = 0;
    loopStart_ = 0;
    loopEnd_ = 0;
    speed_ = 1.f;
}

/**
 * @brief Toggles freeze state.
 */
void Looper::ToggleFreeze()
{
    if (IsFrozen())
    {
        // Not frozen anymore.
        state_ = State::RECORDING;
        loopStart_ = 0;
    }
    else
    {
        // Frozen.
        state_ = State::FROZEN;
        feedbackPickup_ = false;
        if (readPos_ < loopStart_)
        {
            //readPos_ = loopStart_;
        }
    }
}

/**
 * @brief Increments the loop length by the given samples.
 *
 * @param samples
 */
void Looper::IncrementLoopLength(size_t samples)
{
    SetLoopLength((loopLength_ + samples < bufferSamples_) ? loopLength_ + samples : bufferSamples_);
};

/**
 * @brief Decrements the loop length by the given samples.
 *
 * @param samples
 */
void Looper::DecrementLoopLength(size_t samples)
{
    SetLoopLength((loopLength_ > samples) ? loopLength_ - samples : kMinSamples);
};

/**
 * @brief Sets the loop length.
 *
 * @param length
 */
void Looper::SetLoopLength(size_t length)
{
    loopLength_ = length;
    loopEnd_ = loopLength_ - 1;
    loopLengthSeconds_ = loopLength_ / static_cast<float>(sampleRate_);
};

/**
 * @brief Stops the buffering.
 */
void Looper::StopBuffering()
{
    state_ = State::RECORDING;
    loopStart_ = 0;
    writePos_ = 0;
    SetLoopLength(bufferSamples_);
    SetReadPos(forward_ ? loopStart_ : loopEnd_);
}

/**
 * @brief Processes a sample.
 *
 * @param input
 * @param currentSample
 * @return float
 */
float Looper::Process(const float input, const int currentSample)
{
    float output{0.f};

    // Wait a few samples to avoid potential clicking on module's startup.
    if (IsStartingUp())
    {
        fadeIndex_ += 1;
        if (fadeIndex_ > sampleRate_)
        {
            fadeIndex_ = 0;
            state_ = State::BUFFERING;
        }
    }

    // Fill up the buffer the first time.
    if (IsBuffering())
    {
        Write(writePos_, input);
        writePos_ += 1;
        bufferSamples_ = writePos_;
        bufferSeconds_ = bufferSamples_ / static_cast<float>(sampleRate_);

        // Handle end of buffer.
        if (writePos_ > initBufferSamples_ - 1)
        {
            StopBuffering();
        }
    }

    if (IsRecording() || IsFrozen())
    {
        // Received a signal to reset the read position to the loop start point.
        if (mustReset_)
        {
            if (Movement::RANDOM == movement_)
            {
                nextReadPos_ = GetRandomPosition();
                forward_ = nextReadPos_ > readPos_;
            }
            else
            {
                if (forward_)
                {
                    SetReadPosAtStart();
                }
                else
                {
                    SetReadPosAtEnd();
                }
            }
            //fadePos_ = readPos_;
            //fadeIndex_ = 0;
            //mustFade_ = true;
            //mustReset_ = false;
        }

        output = Read(readPos_);

        if (Mode::MIMEO == mode_)
        {
            if (IsFrozen())
            {
                // When frozen, the feedback knob sets the starting point. No
                // writing is done.
                size_t start = std::floor(feedback_ * bufferSamples_);
                // Pick up where the loop start point is.
                if (std::abs(static_cast<int>(start - loopStart_)) < static_cast<int>(bufferSamples_ * 0.1f) && !feedbackPickup_)
                {
                    feedbackPickup_ = true;
                }
                if (feedbackPickup_)
                {
                    loopStart_ = start;
                }
                if (loopStart_ + loopLength_ > bufferSamples_)
                {
                    loopEnd_ = loopStart_ + loopLength_ - bufferSamples_;
                }
                else
                {
                    loopEnd_ = loopStart_ + loopLength_ - 1;
                }
            }
            else
            {
                Write(writePos_, SoftLimit(input + (output * feedback_)));
            }
        }
        else
        {
            float output2{Read(writePos_)};
            // In this mode there always is writing, but when frozen writes the
            // looped signal.
            float writeSig{IsFrozen() ? output : input + (output2 * feedback_)};
            Write(writePos_, SoftLimit(writeSig));
        }

        // Always write forward at original speed.
        writePos_ += 1;
        if (writePos_ > loopEnd_)
        {
            writePos_ = loopStart_;
        }

        float pos{readPos_};
        float coeff{speed_};
        if (Movement::RANDOM == movement_)
        {
            // In this case we just choose randomly the next position.
            if (std::abs(pos - nextReadPos_) < loopLength_ * 0.01f)
            {
                nextReadPos_ = GetRandomPosition();
                forward_ = nextReadPos_ > pos;
            }
            coeff = 1.0f / ((2.f - speed_) * sampleRate_);
        }
        else
        {
            // Otherwise, move the reading position normally.
            nextReadPos_ = forward_ ? readPos_ + speed_ : readPos_ - speed_;
        }

        if (Movement::DRUNK == movement_)
        {
            // When drunk there's a small probability of changing direction.
            if ((rand() % sampleRate_) == 1)
            {
                forward_ = !forward_;
            }
        }

        // Move smoothly to the next position;
        fonepole(pos, nextReadPos_, coeff);
        SetReadPos(pos);

        // Handle normal loop boundaries.
        if (loopEnd_ > loopStart_)
        {
            // Forward direction.
            if (forward_ && readPos_ > loopEnd_)
            {
                SetReadPosAtStart();
            }
            // Backwards direction.
            else if (!forward_ && readPos_ < loopStart_)
            {
                SetReadPosAtEnd();
            }
        }
        // Handle inverted loop boundaries (end point comes before start point).
        else
        {
            if (forward_)
            {
                if (readPos_ > bufferSamples_)
                {
                    // Wrap-around.
                    SetReadPos(0);
                }
                else if (readPos_ > loopEnd_ && readPos_ < loopStart_)
                {
                    SetReadPosAtStart();
                }
            }
            else
            {
                if (readPos_ < 0)
                {
                    // Wrap-around.
                    SetReadPos(bufferSamples_ - 1);
                }
                else if (readPos_ > loopEnd_ && readPos_ < loopStart_)
                {
                    SetReadPosAtEnd();
                }
            }
        }
    }

    return output;
}

/**
 * @brief Reads from a specified point in the delay line using linear
 *        interpolation. Also applies a fade in and out at the beginning and end
 *        of the loop.
 *
 * @param pos
 * @return float
 */
float Looper::Read(float pos)
{
    // 1) get the value at position.

    // Integer position.
    uint32_t i_idx{static_cast<uint32_t>(pos)};
    // Value at the integer position.
    float val{buffer_[i_idx]};
    // Position decimal part.
    float frac{pos - i_idx};
    // If the position is not an integer number we need to interpolate the value.
    if (frac != 0.f)
    {
        // Value at the position after or before, depending on the direction
        // we're going.
        float val2{buffer_[static_cast<uint32_t>(fclamp(i_idx + (forward_ ? 1 : -1), 0, bufferSamples_ - 1))]};
        // Interpolated value.
        val = val + (val2 - val) * frac;
    }

    // 2) fade the value if needed.
    if (mustFade_)
    {
        // The number of samples we need to fade.
        float samples{(loopLength_ > kFadeSamples * 2) ? kFadeSamples : loopLength_ / 2.f};
        cf.SetPos(fadeIndex_ * (1.f / samples));
        float from{};
        float to{1.f};
        val *= cf.Process(from, to);
        fadeIndex_++;
        if (fadeIndex_ > samples)
        {
            mustFade_ = false;
        }
    }

    /*
    // The number of samples we need to fade.
    float samples{loopLength_ > 1200 ? 1200 : loopLength_ / 2.f};
    samples = 10000;
    float kWindowFactor{1.f / samples};

    if (mustFade_)
    {
        float multiplier = 0.5 * (1 - std::cos(2 * PI_F * (fadePos_ + fadeIndex_) / samples));
        //val = std::sin(HALFPI_F * (fadePos_ + fadeIndex_) * kWindowFactor) * val;
        val *= multiplier;
        fadeIndex_++;
        if (fadeIndex_ > samples)
        {
            mustFade_ = false;
        }
    }
    */
    /*
    if (forward_)
    {
        // When going forward we consider read and write position aligned.
        if (pos < samples)
        {
            // Fade in.
            val = std::sin(HALFPI_F * pos * kWindowFactor) * val;
        }
        else if (pos >= loopLength_ - samples)
        {
            // Fade out.
            val = std::sin(HALFPI_F * (loopLength_ - pos) * kWindowFactor) * val;
        }
    }
    else
    {
        // When going backwards read and write position cross in the middle and
        // at beginning/end.
        float diff{pos - writePos_};
        if (diff > 0 && diff < samples)
        {
            // Fade in.
            val = std::sin(HALFPI_F * diff * kWindowFactor) * val;
        }
    }

    */

    return val;
}

void Looper::SetReadPos(float pos)
{
    readPos_ = pos;
    readPosSeconds_ = readPos_ / sampleRate_;

    uint32_t i_idx = static_cast<uint32_t>(readPos_);

    // If we're going forward and are at the beginning of the loop, or we're
    // going backwards and are at the end of the loop or we've been instructed
    // to reset the starting position, set up the fade in.
    if ((forward_ && i_idx == loopStart_) || (!forward_ && i_idx == loopEnd_) || mustReset_)
    {
        fadePos_ = readPos_;
        fadeIndex_ = 0;
        mustFade_ = true;
        mustReset_ = false;
    }
}

size_t Looper::GetRandomPosition()
{
    size_t nextPos{loopStart_ + rand() % (loopLength_ - 1)};
    if (forward_ && nextPos > loopEnd_)
    {
        nextPos = loopEnd_;
    }
    else if (!forward_ && nextPos < loopStart_)
    {
        nextPos = loopStart_;
    }

    return nextPos;
}

void Looper::SetReadPosAtStart()
{
    SetReadPos(loopStart_);
    // Invert direction when in pendulum.
    if (Movement::PENDULUM == movement_)
    {
        forward_ = !forward_;
        SetReadPos(loopEnd_);
    }
}

void Looper::SetReadPosAtEnd()
{
    SetReadPos(loopEnd_);
    // Invert direction when in pendulum.
    if (Movement::PENDULUM == movement_)
    {
        forward_ = !forward_;
        SetReadPos(loopStart_);
    }
}