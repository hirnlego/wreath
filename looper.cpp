#include "looper.h"
#include "dsp.h"

using namespace wreath;
using namespace daisysp;

void Looper::Init(size_t sampleRate, float *mem, int maxBufferSeconds)
{
    sampleRate_ = sampleRate;
    buffer_ = mem;
    initBufferSamples_ = sampleRate * maxBufferSeconds;
    ResetBuffer();

    state_ = State::INIT;
    mode_ = Mode::MIMEO;
    direction_ = Direction::FORWARD;
    forward_ = Direction::FORWARD == direction_;
}

void Looper::SetSpeed(float speed)
{
    speed_ = fclamp(speed, 0.f, 2.f);
}

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

void Looper::IncrementLoopLength(size_t step)
{
    if (loopLength_ < bufferSamples_)
    {
        size_t length{loopLength_ + step};
        if (length > bufferSamples_)
        {
            length = bufferSamples_;
        }
        SetLoopLength(length);
    }
};

void Looper::DecrementLoopLength(size_t step)
{
    if (loopLength_ > 0)
    {
        size_t length{loopLength_ - step};
        if (length < 0)
        {
            length = 0;
        }
        SetLoopLength(length);
    }
};

void Looper::SetLoopLength(size_t length)
{
    loopLength_ = length;
    loopEnd_ = loopLength_ - 1;
};

void Looper::StopBuffering()
{
    loopLength_ = bufferSamples_;
    loopStart_ = 0;
    loopEnd_ = loopLength_ - 1;
    SetReadPos(forward_ ? loopStart_ : loopEnd_);
    writePos_ = 0;
    state_ = State::RECORDING;
}

float Looper::Process(const float input, const int currentSample)
{
    float output{0.f};

    // Wait a few samples to avoid potential clicking on startup.
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
        bufferSeconds_ = bufferSamples_ / (float)sampleRate_;

        // Handle end of buffer.
        if (writePos_ > initBufferSamples_ - 1)
        {
            StopBuffering();
        }
    }

    if (IsRecording() || IsFrozen())
    {
        output = Read(readPos_);

        if (Mode::MIMEO == mode_)
        {
            if (IsFrozen())
            {
                // When frozen, the feedback knob sets the starting point. No writing is done.
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
            // In this mode there always is writing, but when frozen writes the looped signal.
            float writeSig{IsFrozen() ? output : input + (output2 * feedback_)};
            Write(writePos_, SoftLimit(writeSig));
        }
        
        // Always write forward at original speed.
        writePos_ += 1;
        if (writePos_ > loopEnd_)
        {
            writePos_ = loopStart_;
        }

        if (Direction::RANDOM == direction_)
        {
            // In this case we just choose randomly a new starting point.
            SetReadPos(loopStart_ + std::rnd() % loopLength_);
        } 
        else {
            // Otherwise, move the reading position normally.
            SetReadPos(forward_ ? readPos_ + speed_ : readPos_ - speed_);
        }

        // Handle normal loop boundaries.
        if (loopEnd_ > loopStart_)
        {
            // Forward direction.
            if (forward_ && readPos_ > loopEnd_)
            {
                SetReadPos(loopStart_);
                // Invert direction when in pendulum.
                if (Direction::PENDULUM == direction_)
                {
                    SetReadPos(loopEnd_);
                    forward_ = !forward_;
                }
            }
            // Backwards direction.
            else if (!forward_ && readPos_ < loopStart_)
            {
                SetReadPos(loopEnd_);
                // Invert direction when in pendulum.
                if (Direction::PENDULUM == direction_)
                {
                    SetReadPos(loopStart_);
                    forward_ = !forward_;
                }
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
                    SetReadPos(loopStart_);
                    // Invert direction when in pendulum.
                    if (Direction::PENDULUM == direction_)
                    {
                        SetReadPos(loopEnd_);
                        forward_ = !forward_;
                    }
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
                    SetReadPos(loopEnd_);
                    // Invert direction when in pendulum.
                    if (Direction::PENDULUM == direction_)
                    {
                        SetReadPos(loopStart_);
                        forward_ = !forward_;
                    }
                }
            }
        }

        if (Direction::DRUNK == direction_)
        {
            // When drunk there's a 30% probability of changing direction.
            if ((rnd() % 100) < 30) {
                forward_ = !forward_;
            }
        }
    }

    return output;
}

// Reads from a specified point in the delay line using linear interpolation.
// Also applies a fade in and out to the loop.
float Looper::Read(float pos)
{
    float a, b, frac;
    uint32_t i_idx{static_cast<uint32_t>(pos)};
    frac = pos - i_idx;
    a = buffer_[i_idx];
    b = buffer_[(i_idx + (forward_ ? 1 : -1)) % static_cast<uint32_t>(loopLength_)];

    float val{a + (b - a) * frac};

    float samples{loopLength_ > 1200 ? 1200 : loopLength_ / 2.f};
    float kWindowFactor{1.f / samples};
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
        // When going backwards read and write position cross in the middle and at beginning/end.
        float diff{pos - writePos_};
        if (diff > 0 && diff < samples)
        {
            // Fade in.
            val = std::sin(HALFPI_F * diff * kWindowFactor) * val;
        }
    }

    return val;
}