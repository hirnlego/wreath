#include "looper.h"
#include "Utility/dsp.h"

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
    Reset();

    movement_ = Movement::FORWARD;
    forward_ = Movement::FORWARD == movement_;

    cf_.Init(CROSSFADE_CPOW);
}

/**
 * @brief Sets the speed multiplier, clamping its value just in case.
 *
 * @param multiplier
 */
void Looper::SetSpeedMult(float multiplier)
{
    speedMult_ = fclamp(multiplier, 0.f, kMaxSpeedMult);
    readSpeed_ = sampleRate_ * speedMult_; // samples/s.
    writeSpeed_ = sampleRate_; // samples/s.
    sampleRateSpeed_ = static_cast<size_t>(sampleRate_ / speedMult_);
}

/**
 * @brief Sets the movement type.
 *
 * @param movement
 */
void Looper::SetMovement(Movement movement)
{
    if (Movement::FORWARD == movement && !forward_)
    {
        forward_ = true;
    }
    else if (Movement::BACKWARDS == movement && forward_)
    {
        forward_ = false;
    }

    movement_ = movement;
}

/**
 * @brief Clears the buffer.
 */
void Looper::ClearBuffer()
{
    std::fill(&buffer_[0], &buffer_[initBufferSamples_ - 1], 0.f);
}

/**
 * @brief Resets the looper.
 */
void Looper::Reset()
{
    ClearBuffer();
    bufferSamples_ = 0;
    bufferSeconds_ = 0.f;
    loopStart_ = 0;
    loopEnd_ = 0;
    loopLength_ = 0;
    loopLengthSeconds_ = 0.f;
    readPos_ = 0.f;
    readPosSeconds_ = 0.f;
    nextReadPos_ = 0.f;
    fadeIndex_ = 0;
    fadePos_ = 0;
    writePos_ = 0;
    speedMult_ = 1.f;
    readSpeed_ = writeSpeed_ = sampleRate_;
}

bool Looper::Buffer(float value)
{
    // Handle end of buffer.
    if (writePos_ > initBufferSamples_ - 1)
    {
        return true;
    }

    Write(value);
    writePos_++;
    bufferSamples_ = writePos_;
    bufferSeconds_ = bufferSamples_ / static_cast<float>(sampleRate_);

    return false;
}

void Looper::Restart()
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
    writePos_ = 0;
}

/**
 * @brief Stops the buffering.
 */
void Looper::StopBuffering()
{
    loopStart_ = 0;
    writePos_ = 0;
    ResetLoopLength();
    SetReadPos(forward_ ? loopStart_ : loopEnd_);
}

/**
 * @brief Sets the loop length.
 *
 * @param length
 */
void Looper::SetLoopLength(size_t length)
{
    loopLength_ = fclamp(length, kMinSamples, bufferSamples_);
    loopEnd_ = loopLength_ - 1;
    loopLengthSeconds_ = loopLength_ / static_cast<float>(sampleRate_);
}

/**
 * @brief Sets the loop length to that of the written buffer.
 */
void Looper::ResetLoopLength()
{
    SetLoopLength(bufferSamples_);
}

/**
 * @brief Returns the number of samples to fade given the provided read
 *        position in the buffer.
 *
 * @param pos
 */
void Looper::CalculateFadeSamples(size_t pos)
{
    if (loopLength_ < kFadeSamples)
    {
        fadeSamples_ = 0;
    }

    else if (forward_ && pos + kFadeSamples > loopEnd_)
    {
        fadeSamples_ = loopEnd_ - static_cast<int>(pos);
    }

    else if (forward_ && pos - kFadeSamples < loopStart_)
    {
        fadeSamples_ = static_cast<int>(pos);
    }

    else
    {
        fadeSamples_ = kFadeSamples;
    }
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
    size_t intPos{static_cast<size_t>(std::round(pos))};
    // Value at the integer position.
    float val{buffer_[intPos]};
    // Position decimal part.
    float frac{pos - intPos};
    // If the position is not an integer number we need to interpolate the value.
    if (frac != 0.f)
    {
        // Value at the position after or before, depending on the direction
        // we're going.
        float val2{buffer_[static_cast<size_t>(fclamp(intPos + (forward_ ? 1 : -1), 0, bufferSamples_ - 1))]};
        // Interpolated value.
        val = val + (val2 - val) * frac;
    }

    // 2) fade the value if needed.
    if (mustFade_ != -1)
    {
        // The number of samples we need to fade.
        cf_.SetPos(fadeIndex_ * (1.f / fadeSamples_));
        float fadeValues[2][2]{{ 0.f, 1.f }, { 1.f, 0.f }};
        val *= cf_.Process(fadeValues[mustFade_][0], fadeValues[mustFade_][1]);
        fadeIndex_ += 1;
        // End and reset the fade when done.
        if (fadeIndex_ > fadeSamples_)
        {
            fadeIndex_ = 0;
            cf_.SetPos(0.f);
            if (Fade::OUT == mustFade_)
            {
                fadeIndex_ = 0;
                fadePos_ = writePos_;
                mustFade_ = Fade::IN;
            }
            else
            {
                crossPointFound_ = false;
                mustFade_ = Fade::NONE;
            }
        }
    }

    return val;
}

/**
 * @brief
 *
 * @param pos
 */
void Looper::SetWritePos(float pos)
{
    writePos_ = (pos > loopEnd_) ? loopStart_ : pos;
}

/**
 * @brief Updates the given position depending on the loop boundaries and the
 *        current movement type.
 *
 * @param pos
 * @param isReadPos
 */
void Looper::HandlePosBoundaries(float &pos, bool isReadPos)
{
    // Handle normal loop boundaries.
    if (loopEnd_ > loopStart_)
    {
        // Forward direction.
        if (forward_ && pos > loopEnd_)
        {
            pos = loopStart_;
            // Invert direction when in pendulum.
            if (Movement::PENDULUM == movement_)
            {
                // Switch direction only if we're handling the read position.
                if (isReadPos) {
                    forward_ = !forward_;
                }
                pos = loopEnd_;
            }
        }
        // Backwards direction.
        else if (!forward_ && pos < loopStart_)
        {
            pos = loopEnd_;
            // Invert direction when in pendulum.
            if (Movement::PENDULUM == movement_)
            {
                // Switch direction only if we're handling the read position.
                if (isReadPos) {
                    forward_ = !forward_;
                }
                pos = loopStart_;
            }
        }
    }
    // Handle inverted loop boundaries (end point comes before start point).
    else
    {
        if (forward_)
        {
            if (pos > bufferSamples_)
            {
                // Wrap-around.
                pos = 0;
            }
            else if (pos > loopEnd_ && pos < loopStart_)
            {
                pos = loopStart_;
                // Invert direction when in pendulum.
                if (Movement::PENDULUM == movement_)
                {
                    // Switch direction only if we're handling the read position.
                    if (isReadPos) {
                        forward_ = !forward_;
                    }
                    pos = loopEnd_;
                }
            }
        }
        else
        {
            if (pos < 0)
            {
                // Wrap-around.
                pos = bufferSamples_ - 1;
            }
            else if (pos > loopEnd_ && pos < loopStart_)
            {
                pos = loopEnd_;
                // Invert direction when in pendulum.
                if (Movement::PENDULUM == movement_)
                {
                    // Switch direction only if we're handling the read position.
                    if (isReadPos) {
                        forward_ = !forward_;
                    }
                    pos = loopStart_;
                }
            }
        }
    }
}

void Looper::CalculateHeadsDistance()
{
    // forward, rp > wp, ws > rs = rp - wp
    // backwards, rp > wp, ws > rs = rp - wp
    // backwards, rp > wp, rs > ws = rp - wp
    if (readPos_ > writePos_ && ((forward_ && writeSpeed_ > readSpeed_) || (!forward_ && readPos_ > writePos_)))
    {
        headsDistance_ = readPos_ - writePos_;
    }

    // forward, rp > wp, rs > ws = b - rp + wp
    else if (forward_ && readPos_ > writePos_)
    {
        headsDistance_ = bufferSamples_ - readPos_ + writePos_;
    }

    // forward, wp > rp, ws > rs = b - wp + rp
    // backwards, wp > rp, ws > rs = b - wp + rp
    // backwards, wp > rp, rs > ws = b - wp + rp
    else if ((forward_ && writePos_ > readPos_ && writeSpeed_ > readSpeed_) || (!forward_ && writePos_ > readPos_))
    {
        headsDistance_ = bufferSamples_ - writePos_ + readPos_;
    }

    // forward, wp > rp, rs > ws = wp - rp
    else if (forward_ && readSpeed_ > writeSpeed_)
    {
        headsDistance_ = writePos_ - readPos_;
    }

    else
    {
        headsDistance_ = 0.f;
    }
}

// Handle read fade.
void Looper::HandleFade()
{
    size_t intPos{static_cast<size_t>(std::round(readPos_))};
    static size_t crossPoint{};

    // When the two heads are going at different speeds, we must calculate
    // the point in which the two will cross each other so we can set up a
    // fading accordingly.
    if ((readSpeed_ != writeSpeed_ || !forward_) && headsDistance_ > 0 && headsDistance_ <= fadeSamples_ * 2 && !crossPointFound_)
    {
        float deltaTime{};
        float relSpeed{writeSpeed_ > readSpeed_ ? writeSpeed_ - readSpeed_ : readSpeed_ - writeSpeed_};
        if (!forward_)
        {
            relSpeed = writeSpeed_ + readSpeed_;
        }
        deltaTime = headsDistance_ / relSpeed;
        crossPoint = writePos_ + (writeSpeed_ * deltaTime) - 1;
        // Wrap the meeting point if it's outside of the buffer.
        if (crossPoint >= bufferSamples_)
        {
            size_t r = crossPoint % loopLength_;
            if (loopStart_ + r > bufferSamples_)
            {
                crossPoint = r - (bufferSamples_ - loopStart_);
            }
            else
            {
                crossPoint = loopStart_ + r;
            }
        }
        temp = crossPoint;
        crossPointFound_ = true;
    }

    // Set up a fade in if:
    // - we're going forward and are right at the beginning of the loop;
    // - we're going backwards and are right at the end of the loop;
    // - we're going backwards, or at a different speed, and the two heads have just met (note that at this point the write position already stepped forward, so we must check against crossPoint + 1).
    if ((forward_ && intPos == loopStart_) ||
        (!forward_ && intPos == loopEnd_))
    {
        fadePos_ = writePos_;
        fadeIndex_ = 0;
        mustFade_ = Fade::IN;
    }

    // Set up a fade out if:
    // - we're going forward and are almost at the end of the loop;
    // - we're going backwards and are almost at the beginning of the loop;
    // - we're going backwards, or at a different speed, and the two heads are about to meet.
    else if ((forward_ && intPos == loopEnd_ - fadeSamples_) ||
        (!forward_ && intPos == loopStart_ + fadeSamples_) ||
        (crossPointFound_ && (writePos_ == crossPoint - fadeSamples_)))
    {
        fadePos_ = writePos_;
        fadeIndex_ = 0;
        mustFade_ = Fade::OUT;
    }
}

/**
 * @brief Sets the read position. Also, sets a fade in/out if needed.
 *
 * @param pos
 */
void Looper::SetReadPos(float pos)
{
    readPos_ = pos;
    readPosSeconds_ = readPos_ / sampleRate_;
    if (readSpeed_ != writeSpeed_ || !forward_)
    {
        CalculateHeadsDistance();
    }
    if (mustFade_ == Fade::NONE)
    {
        CalculateFadeSamples(static_cast<size_t>(std::round(pos)));
        if (fadeSamples_ > 0)
        {
            HandleFade();
        }
    }
}

/**
 * @brief Returns a random position within the loop.
 *
 * @return size_t
 */
size_t Looper::GetRandomPosition()
{
    size_t pos{loopStart_ + rand() % (loopLength_ - 1)};
    if (forward_ && pos > loopEnd_)
    {
        pos = loopEnd_;
    }
    else if (!forward_ && pos < loopStart_)
    {
        pos = loopStart_;
    }

    return pos;
}

/**
 * @brief Sets the read position at the beginning of the loop.
 */
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

/**
 * @brief Sets the read position at the end of the loop.
 */
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