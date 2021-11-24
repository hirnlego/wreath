#include "looper.h"

using namespace wreath;
using namespace daisysp;

void Looper::Init(float *mem, size_t size)
{
    buffer_ = mem;
    initBufferSize_ = size;
    ResetBuffer();

    stage_ = -1;
    mode_ = Mode::MIMEO;
    direction_ = Direction::FORWARD;
    forward_ = Direction::FORWARD == direction_;
}

void Looper::ResetBuffer()
{
    std::fill(&buffer_[0], &buffer_[initBufferSize_ - 1], 0);
    bufferSize_ = 0;
    stage_ = 0;
    freeze_ = false;
    feedback_ = 0;
    feedbackPickup_ = false;
    readPos_ = 0;
    writePos_ = 0;
    loopStart_ = 0;
    loopEnd_ = 0;
    speed_ = 1.f;
}

void Looper::ToggleFreeze()
{
    freeze_ = !freeze_;
    if (freeze_)
    {
        feedbackPickup_ = false;
        // Frozen.
        if (readPos_ < loopStart_)
        {
            //readPos_ = loopStart_;
        }
    }
    else
    {
        // Not frozen anymore.
        loopStart_ = 0;
    }
}

void Looper::ChangeLoopLength(float length)
{
    loopLength_ = length;
    loopEnd_ = loopLength_ - 1;
}

void Looper::StopBuffering()
{
    loopLength_ = bufferSize_;
    loopStart_ = 0;
    loopEnd_ = loopLength_ - 1;
    readPos_ = forward_ ? loopStart_ : loopEnd_;
    writePos_ = 0;
    stage_++;
}

float Looper::Process(const float input, const int currentSample)
{
    float readSig = 0.f;

    // Wait a few samples to avoid potential clicking on startup.
    if (-1 == stage_)
    {
        fadeIndex_ += 1;
        if (fadeIndex_ > 48000)
        {
            stage_++;
        }
    }

    // Fill up the buffer the first time.
    if (0 == stage_)
    {
        Write(writePos_, input);
        writePos_ += 1;
        bufferSize_ = writePos_;

        // Handle end of buffer.
        if (writePos_ > initBufferSize_ - 1)
        {
            StopBuffering();
        }
    }

    if (1 == stage_)
    {
        readSig = Read(readPos_);

        if (Mode::MIMEO == mode_)
        {
            if (freeze_)
            {
                // When frozen, the feedback knob sets the starting point. No writing is done.
                size_t start = std::floor(feedback_ * bufferSize_);
                // Pick up where the loop start point is.
                if (std::abs((int)start - (int)loopStart_) < MIN_LENGTH && !feedbackPickup_)
                {
                    feedbackPickup_ = true;
                }
                if (feedbackPickup_)
                {
                    loopStart_ = start;
                }
                if (loopStart_ + loopLength_ > bufferSize_)
                {
                    loopEnd_ = loopStart_ + loopLength_ - bufferSize_;
                }
                else
                {
                    loopEnd_ = loopStart_ + loopLength_ - 1;
                }
            }
            else
            {
                Write(writePos_, SoftLimit(input + (readSig * feedback_)));
            }

            // Always write forward at original speed.
            writePos_ += 1;
            if (writePos_ > loopEnd_)
            {
                writePos_ = loopStart_;
            }
        }
        else
        {
            float readSig2 = Read(writePos_);
            // In this mode there always is writing, but when frozen writes the looped signal.
            float writeSig = freeze_ ? readSig : input + (readSig2 * feedback_);
            Write(writePos_, SoftLimit(writeSig));
            // Always write forward at single speed.
            writePos_ += 1;
            if (writePos_ > bufferSize_ - 1)
            {
                writePos_ = 0;
            }
        }

        // Move the reading position.
        readPos_ = forward_ ? readPos_ + speed_ : readPos_ - speed_;

        // Handle normal loop boundaries.
        if (loopEnd_ > loopStart_)
        {
            // Forward direction.
            if (forward_ && readPos_ > loopEnd_)
            {
                readPos_ = loopStart_;
                // Invert direction when in pendulum.
                if (Direction::PENDULUM == direction_)
                {
                    readPos_ = loopEnd_;
                    forward_ = !forward_;
                }
            }
            // Backwards direction.
            else if (!forward_ && readPos_ < loopStart_)
            {
                readPos_ = loopEnd_;
                // Invert direction when in pendulum.
                if (Direction::PENDULUM == direction_)
                {
                    readPos_ = loopStart_;
                    forward_ = !forward_;
                }
            }
        }
        // Handle inverted loop boundaries (end point comes before start point).
        else
        {
            if (forward_)
            {
                if (readPos_ > bufferSize_)
                {
                    // Wrap-around.
                    readPos_ = 0;
                }
                else if (readPos_ > loopEnd_ && readPos_ < loopStart_)
                {
                    readPos_ = loopStart_;
                    // Invert direction when in pendulum.
                    if (Direction::PENDULUM == direction_)
                    {
                        readPos_ = loopEnd_;
                        forward_ = !forward_;
                    }
                }
            }
            else
            {
                if (readPos_ < 0)
                {
                    // Wrap-around.
                    readPos_ = bufferSize_ - 1;
                }
                else if (readPos_ > loopEnd_ && readPos_ < loopStart_)
                {
                    readPos_ = loopEnd_;
                    // Invert direction when in pendulum.
                    if (Direction::PENDULUM == direction_)
                    {
                        readPos_ = loopStart_;
                        forward_ = !forward_;
                    }
                }
            }
        }
    }

    return readSig;
}

// Reads from a specified point in the delay line using linear interpolation.
// Also applies a fade in and out to the loop.
float Looper::Read(float pos, bool fade = true)
{
    float a, b, frac;
    uint32_t i_idx = static_cast<uint32_t>(pos);
    frac = pos - i_idx;
    a = buffer_[i_idx];
    b = buffer_[(i_idx + (forward_ ? 1 : -1)) % static_cast<uint32_t>(loopLength_)];

    float val = a + (b - a) * frac;

    float samples = loopLength_ > 1200 ? 1200 : loopLength_ / 2.f;
    float kWindowFactor = (1.f / samples);
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
        float diff = pos - writePos_;
        if (diff > 0 && diff < samples)
        {
            // Fade in.
            val = std::sin(HALFPI_F * diff * kWindowFactor) * val;
        }
    }

    return val;
}

void Looper::Write(size_t pos, float value)
{
    buffer_[pos] = value;
}