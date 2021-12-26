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
void Looper::Init(int32_t sampleRate, float *mem, int32_t maxBufferSamples)
{
    sampleRate_ = sampleRate;
    buffer_ = mem;
    maxBufferSamples_ = maxBufferSamples;
    Reset();
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
    sampleRateSpeed_ = static_cast<int32_t>(sampleRate_ / speedMult_);
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
    movement_ = Movement::FORWARD;
    forward_ = Movement::FORWARD == movement_;
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
    int32_t startPositions[]{loopEnd_, loopStart_};
    // Invert direction when in pendulum.
    if (Movement::PENDULUM == movement_ || Movement::DRUNK == movement_)
    {
        forward_ = !forward_;
    }
    if (Movement::DRUNK != movement_)
    {
        writePos_ = loopStart_;
        int32_t pos = FindMinValPos(startPositions[forward_]);
        SetReadPos(pos);
    }
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

void Looper::SetLoopStart(int32_t pos)
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
void Looper::SetLoopLength(int32_t length)
{
    loopLength_ = fclamp(length, kMinLoopLengthSamples, bufferSamples_);
    loopLengthSeconds_ = loopLength_ / static_cast<float>(sampleRate_);
    UpdateLoopEnd();
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

}

/**
 * @brief
 *
 * @param pos
 */
void Looper::SetWritePos(int32_t pos)
{
    writePos_ = (pos > loopEnd_) ? loopStart_ : pos;
}

float Looper::FindMinValPos(float pos)
{
    float min{};
    float minPos{pos};
    float value{buffer_[static_cast<int32_t>(std::round(pos))]};

	for (int i = 0; i < 10; i++)
	{
        pos = pos + (forward_ ? i : -i);
        HandlePosBoundaries(pos);
        float val = buffer_[static_cast<int32_t>(std::round(pos))];
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
		sign1 = buffer_[static_cast<int32_t>(std::round(pos))] > 0;
        float pos2 = pos1 + 1;
        HandlePosBoundaries(pos2);
		sign2 = buffer_[static_cast<int32_t>(std::round(pos2))] > 0;
		if (sign1 != sign2)
        {
			return pos1;
        }
	}

    return pos;
}

void Looper::CalculateFadeSamples(int32_t pos)
{
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
    int32_t intPos{static_cast<int32_t>(std::round(readPos_))};
    static int32_t crossPoint{};

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
            int32_t r = crossPoint % loopLength_;
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
    if ((forward_ && writePos_ == loopStart_) ||
        (!forward_ && writePos_ == loopEnd_))
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
            CalculateFadeSamples(static_cast<int32_t>(std::round(pos)));
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
 * @return int32_t
 */
int32_t Looper::GetRandomPosition()
{
    int32_t pos{loopStart_ + rand() % (loopLength_ - 1)};
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