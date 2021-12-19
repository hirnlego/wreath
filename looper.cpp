#include "looper.h"
#include "Utility/dsp.h"
#include <cstring>

using namespace wreath;
using namespace daisysp;

/**
 * @brief Initializes the looper.
 *
 * @param sampleRate
 * @param mem
 * @param maxBufferSamples
 */
void Looper::Init(size_t sampleRate, float *mem, size_t maxBufferSamples)
{
    sampleRate_ = sampleRate;
    buffer_ = mem;
    maxBufferSamples_ = maxBufferSamples;
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
    speedMult_ = multiplier;
    readSpeed_ = sampleRate_ * speedMult_; // samples/s.
    writeSpeed_ = sampleRate_;             // samples/s.
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
    //std::fill(&buffer_[0], &buffer_[maxBufferSamples_ - 1], 0.f);
    memset(buffer_, 0.f, maxBufferSamples_);
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
    loopStartSeconds_ = 0.f;
    loopLengthSeconds_ = 0.f;
    readPos_ = 0.f;
    readPosSeconds_ = 0.f;
    nextReadPos_ = 0.f;
    fadeIndex_ = 0;
    //fadePos_ = 0;
    writePos_ = 0;
    SetSpeedMult(1.f);
}

bool Looper::Buffer(float value)
{
    // Handle end of buffer.
    if (writePos_ > maxBufferSamples_ - 1)
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
    forward_ ? SetReadPosAtStart() : SetReadPosAtEnd();
    writePos_ = 0;
}

/**
 * @brief Stops the buffering.
 */
void Looper::StopBuffering()
{
    loopStart_ = 0;
    loopStartSeconds_ = 0.f;
    writePos_ = 0;
    SetLoopLength(bufferSamples_);
    SetReadPos(forward_ ? loopStart_ : loopEnd_);
}

void Looper::SetLoopStart(size_t pos)
{
    loopStart_ = fclamp(pos, 0, bufferSamples_ - 1);
    loopStartSeconds_ = loopStart_ / static_cast<float>(sampleRate_);
    UpdateLoopEnd();
};

void Looper::UpdateLoopEnd()
{
    if (loopStart_ + loopLength_ > bufferSamples_)
    {
        loopEnd_ = (loopStart_ + loopLength_) - bufferSamples_ - 1;
    }
    else
    {
        loopEnd_ = loopStart_ + loopLength_ - 1;
    }
}

/**
 * @brief Sets the loop length.
 *
 * @param length
 */
void Looper::SetLoopLength(size_t length)
{
    loopLength_ = fclamp(length, kMinLoopLengthSamples, bufferSamples_);
    loopLengthSeconds_ = loopLength_ / static_cast<float>(sampleRate_);
    UpdateLoopEnd();
}

/**
 * @brief Returns the number of samples to fade given the provided read
 *        position in the buffer.
 *
 * @param pos
 */
void Looper::CalculateFadeSamples(size_t pos)
{
    fadeSamples_ = kSamplesToFade;
    /*
    if (loopLength_ < kSamplesToFade)
    {
        fadeSamples_ = 0;
    }

    else if (forward_ && pos + kSamplesToFade > loopEnd_)
    {
        fadeSamples_ = loopEnd_ - static_cast<int>(pos);
    }

    else if (forward_ && pos - kSamplesToFade < loopStart_)
    {
        fadeSamples_ = static_cast<int>(pos);
    }

    else
    {
        fadeSamples_ = kSamplesToFade;
    }
    */
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
    float pos2 = pos + (forward_ ? 1.f : -1.f);
    HandlePosBoundaries(pos2);
    float val2{buffer_[static_cast<size_t>(std::round(pos2))]};
    // Interpolated value.
    val = val + (val2 - val) * frac;

    // 2) fade the value if needed.
    if (mustFade_ != -1)
    {
        cf_.SetPos(fadeIndex_ * (1.f / fadeSamples_));
        float fadeValues[2][2]{{val, 0.f}, {0.f, val}};
        val = cf_.Process(fadeValues[mustFade_][0], fadeValues[mustFade_][1]);
        fadeIndex_ += 1;
        // End and reset the fade when done.
        if (fadeIndex_ > fadeSamples_)
        {
            fadeIndex_ = 0;
            cf_.SetPos(0.f);
            // After a fade out there's always a fade in.
            if (Fade::OUT == mustFade_)
            {
                fadeIndex_ = 0;
                //fadePos_ = writePos_;
                mustFade_ = Fade::IN;
                val = 0;
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
void Looper::SetWritePos(size_t pos)
{
    writePos_ = (pos > loopEnd_) ? loopStart_ : pos;
}

float Looper::FindMinValPos(float pos)
{
    float min{};
    float minPos{pos};
    float value{buffer_[static_cast<size_t>(std::round(pos))]};

	for (int i = 0; i < 10; i++)
	{
        pos += i;
        HandlePosBoundaries(pos);
        float val = buffer_[static_cast<size_t>(std::round(pos))];
        if (std::abs(value - val) < min)
        {
            min = val;
            minPos = pos;
        }
	}

    return minPos;
}


float Looper::ZeroCrossingPos(float pos)
{
	int i;
	bool sign1, sign2;

	for (i = 0; i < kSamplesToFade; i++)
	{
        float pos1 = pos + i;
        HandlePosBoundaries(pos1);
		sign1 = buffer_[static_cast<size_t>(std::round(pos))] > 0;
        float pos2 = pos1 + 1;
        HandlePosBoundaries(pos2);
		sign2 = buffer_[static_cast<size_t>(std::round(pos2))] > 0;
		if (sign1 != sign2)
        {
			return pos1;
        }
	}

    return pos;
}

/**
 * @brief Updates the given position depending on the loop boundaries and the
 *        current movement type. Returns true if the position has been altered.
 *
 * @param pos
 * @return true
 * @return false
 */
bool Looper::HandlePosBoundaries(float &pos)
{
    // Handle normal loop boundaries.
    if (loopEnd_ > loopStart_)
    {
        // Forward direction.
        if (forward_ && pos > loopEnd_)
        {
            pos = loopStart_ + (pos - loopEnd_) - 1;
            // Invert direction when in pendulum.
            if (Movement::PENDULUM == movement_)
            {
                pos = loopEnd_ - pos;
                forward_ = !forward_;
            }

            return true;
        }
        // Backwards direction.
        else if (!forward_ && pos < loopStart_)
        {
            pos = loopEnd_ - (loopStart_ - pos) + 1;
            // Invert direction when in pendulum.
            if (Movement::PENDULUM == movement_)
            {
                pos = loopStart_ + std::abs(pos);
                forward_ = !forward_;
            }
            return true;
        }
    }
    // Handle inverted loop boundaries (end point comes before start point).
    else
    {
        if (forward_)
        {
            if (pos > bufferSamples_ - 1)
            {
                // Wrap-around.
                pos = pos - bufferSamples_;

                return true;
            }
            else if (pos > loopEnd_ && pos < loopStart_)
            {
                pos = loopStart_;
                // Invert direction when in pendulum.
                if (Movement::PENDULUM == movement_)
                {
                    pos = loopEnd_;
                    forward_ = !forward_;
                }

                return true;
            }
        }
        else
        {
            if (pos < 0)
            {
                // Wrap-around.
                pos = (bufferSamples_ - 1) + pos;

                return true;
            }
            else if (pos > loopEnd_ && pos < loopStart_)
            {
                pos = loopEnd_;
                // Invert direction when in pendulum.
                if (Movement::PENDULUM == movement_)
                {
                    pos = loopStart_;
                    forward_ = !forward_;
                }

                return true;
            }
        }
    }

    return false;
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

    if (headsDistance_ <= fadeSamples_ * 2)
    {
        headsDistance_ = headsDistance_;
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
    if ((readSpeed_ != writeSpeed_ || !forward_) && headsDistance_ > 0 && headsDistance_ <= fadeSamples_ * 2 && !crossPointFound_ && writingActive_)
    {
        float deltaTime{};
        float relSpeed{writeSpeed_ > readSpeed_ ? writeSpeed_ - readSpeed_ : readSpeed_ - writeSpeed_};
        if (!forward_)
        {
            relSpeed = writeSpeed_ + readSpeed_;
        }
        deltaTime = headsDistance_ / relSpeed;
        crossPoint = writePos_ + (writeSpeed_ * deltaTime);
        // Wrap the crossing point if it's outside of the buffer.
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
        fadeSamples_ = headsDistance_ / 2;
    }

    // Set up a fade in if:
    // - we're going forward and are right at the beginning of the loop;
    // - we're going backwards and are right at the end of the loop;
    // - we're going backwards, or at a different speed, and the two heads have just met (note that at this point the write position already stepped forward, so we must check against crossPoint + 1).
    if ((forward_ && intPos == loopStart_) ||
        (!forward_ && intPos == loopEnd_))
    {
        //fadePos_ = writePos_;
        //fadeIndex_ = 0;
        //mustFade_ = Fade::IN;
    }

    // Set up a fade out if:
    // - we're going forward and are almost at the end of the loop;
    // - we're going backwards and are almost at the beginning of the loop;
    // - we're going backwards, or at a different speed, and the two heads are about to meet.
    else if ((crossPointFound_ && (writePos_ == crossPoint - fadeSamples_)))
    {
        //fadePos_ = writePos_;
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
    if (readingActive_ && writingActive_)
    {
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