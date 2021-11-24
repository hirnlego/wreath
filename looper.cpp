#include "looper.h"
#include "dsp.h"

using namespace wreath;
using namespace daisysp;

void Looper::Init(float *mem, size_t size)
{
    buffer_ = mem;
    initBufferSamples_ = size;
    ResetBuffer();

    stage_ = -1;
    mode_ = Mode::MIMEO;
    direction_ = Direction::FORWARD;
    forward_ = Direction::FORWARD == direction_;
}

void Looper::SetSpeed(float speed) 
{ 
    speed_ = fclamp(speed, 0, MAX_SPEED); 
}

void Looper::ResetBuffer()
{
    std::fill(&buffer_[0], &buffer_[initBufferSamples_ - 1], 0);
    bufferSamples_ = 0;
    bufferSeconds_ = 0;
    stage_ = 0;
    freeze_ = false;
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

void Looper::StopBuffering()
{
    bufferSeconds_ = bufferSamples_ / 48000.f;
    loopLength_ = bufferSamples_;
    loopStart_ = 0;
    loopEnd_ = loopLength_ - 1;
    SetReadPos(forward_ ? loopStart_ : loopEnd_);
    writePos_ = 0;
    stage_++;
}

float Looper::Process(const float input, const int currentSample)
{
    float output = 0.f;

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
        bufferSamples_ = writePos_;

        // Handle end of buffer.
        if (writePos_ > initBufferSamples_ - 1)
        {
            StopBuffering();
        }
    }

    if (1 == stage_)
    {
        output = Read(readPos_);

        if (Mode::MIMEO == mode_)
        {
            if (freeze_)
            {
                // When frozen, the feedback knob sets the starting point. No writing is done.
                size_t start = std::floor(feedback_ * bufferSamples_);
                // Pick up where the loop start point is.
                if (std::abs((int)start - (int)loopStart_) < MIN_LENGTH && !feedbackPickup_)
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

            // Always write forward at original speed.
            writePos_ += 1;
            if (writePos_ > loopEnd_)
            {
                writePos_ = loopStart_;
            }
        }
        else
        {
            float output2 = Read(writePos_);
            // In this mode there always is writing, but when frozen writes the looped signal.
            float writeSig = freeze_ ? output : input + (output2 * feedback_);
            Write(writePos_, SoftLimit(writeSig));
            // Always write forward at single speed.
            writePos_ += 1;
            if (writePos_ > bufferSamples_ - 1)
            {
                writePos_ = 0;
            }
        }

        // Move the reading position.
        SetReadPos(forward_ ? readPos_ + speed_ : readPos_ - speed_);

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
    }

    return output;
}

// Reads from a specified point in the delay line using linear interpolation.
// Also applies a fade in and out to the loop.
float Looper::Read(float pos)
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