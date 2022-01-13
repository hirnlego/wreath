#include "looper.h"
#include "Utility/dsp.h"

using namespace wreath;
using namespace daisysp;

/**
 * @brief Initializes the looper.
 *
 * @param sampleRate
 * @param buffer
 * @param maxBufferSamples
 */
void Looper::Init(int32_t sampleRate, float *buffer, int32_t maxBufferSamples)
{
    sampleRate_ = sampleRate;
    heads_[READ].Init(buffer, maxBufferSamples);
    heads_[WRITE].Init(buffer, maxBufferSamples);
    cf_.Init(CROSSFADE_CPOW);
    Reset();
}

void Looper::Reset()
{
    heads_[READ].Reset();
    heads_[WRITE].Reset();
    bufferSamples_ = 0;
    bufferSeconds_ = 0.f;
    loopStart_ = 0;
    loopStartSeconds_ = 0.f;
    loopEnd_ = 0;
    loopLength_ = 0;
    loopLengthSeconds_ = 0.f;
    readPos_ = 0.f;
    readPosSeconds_ = 0.f;
    nextReadPos_ = 0.f;
    writePos_ = 0;
    fadeIndex_ = 0;
    movement_ = Movement::NORMAL;
    direction_ = Direction::FORWARD;
    writeRate_ = 1.f;
    SetReadRate(1.f);
}

void Looper::ClearBuffer()
{
    heads_[WRITE].ClearBuffer();
}
float prevValue{};
float currentValue{};
bool Looper::Buffer(float value)
{
    //bool end = heads_[WRITE].Buffer(value * fadeIndex_);
    bool end = heads_[WRITE].Buffer(value);
    currentValue = value;
    prevValue = value;
    bufferSamples_ = heads_[WRITE].GetBufferSamples();
    bufferSeconds_ = bufferSamples_ / static_cast<float>(sampleRate_);
    if (fadeIndex_ < 1.f)
    {
        fadeIndex_ += (1.f / kSamplesToFade);
    }

    return end;
}

void Looper::StopBuffering()
{
    heads_[READ].InitBuffer(heads_[WRITE].StopBuffering());
    loopStart_ = 0;
    loopStartSeconds_ = 0.f;
    loopEnd_ = bufferSamples_ - 1 ;
    loopLength_ = bufferSamples_;
    loopLengthSeconds_ = loopLength_ / static_cast<float>(sampleRate_);


    // DEBUG
    //direction_ = heads_[READ].ToggleDirection();
    //heads_[READ].ResetPosition();
}

void Looper::Restart()
{
    heads_[READ].ResetPosition();
    heads_[WRITE].ResetPosition();
    // Invert direction when in pendulum.
    if (Movement::PENDULUM == movement_ || Movement::DRUNK == movement_)
    {
        direction_ = heads_[READ].ToggleDirection();
    }
}

void Looper::SetLoopStart(int32_t pos)
{
    loopStart_ = heads_[READ].SetLoopStart(pos);
    heads_[WRITE].SetLoopStart(pos);
    loopStartSeconds_ = loopStart_ / static_cast<float>(sampleRate_);
    loopEnd_ = heads_[READ].GetLoopEnd();
    crossPointFound_ = false;
    mustFade_ = false;
};

void Looper::SetLoopEnd(int32_t pos)
{
    loopEnd_ = pos;
}

void Looper::SetLoopLength(int32_t length)
{
    loopLength_ = heads_[READ].SetLoopLength(length);
    heads_[WRITE].SetLoopLength(length);
    loopLengthSeconds_ = loopLength_ / static_cast<float>(sampleRate_);
    loopEnd_ = heads_[READ].GetLoopEnd();
    crossPointFound_ = false;
    mustFade_ = false;
}

void Looper::SetReadRate(float rate)
{
    heads_[READ].SetRate(rate);
    readRate_ = rate;
    readSpeed_ = sampleRate_ * readRate_; // samples/s.
    writeSpeed_ = sampleRate_ * writeRate_;             // samples/s.
    sampleRateSpeed_ = static_cast<int32_t>(sampleRate_ / readRate_);
    crossPointFound_ = false;
    mustFade_ = false;
}

void Looper::SetMovement(Movement movement)
{
    heads_[READ].SetMovement(movement);
    movement_ = movement;
}

void Looper::SetDirection(Direction direction)
{
    heads_[READ].SetDirection(direction);
    direction_ = direction;
}

void Looper::SetReadPosition(float position)
{
    heads_[READ].SetIndex(position);
    readPos_ = position;
}

float Looper::Read()
{
    float value = heads_[READ].Read();
    return value;
    if (mustFade_)
    {
        //bool equal = std::fabs(value - prevValue) <= ( (std::fabs(value) < std::fabs(prevValue) ? std::fabs(prevValue) : std::fabs(value)) * 0.005f);
        bool aPos = value > 0;
        bool bPos = prevValue > 0;
        bool equal = std::fabs(std::max(value, prevValue) - std::min(value, prevValue)) <= 0.001f && aPos == bPos;
        if (equal)
        {
           //mustFade_ = false;
        }
        float delta = std::fabs(std::max(value, prevValue) - std::min(value, prevValue));
        delta *= bPos ? 1 : -1;
        value += prevValue * (1.f - (fadeIndex_ / kSamplesToFade));
    }
    else
    {
        prevValue = currentValue;
        currentValue = value;
    }

    return value;
}

void Looper::Write(float dry, float wet)
{
    float value{};
    if (mustFade_)
    {
        value = dry;// + prevValue;
        fadeIndex_ += readRate_;
        if (fadeIndex_ > kSamplesToFade)
        {
            mustFade_ = false;
            crossPointFound_ = false;
        }
    }
    else
    {
       value = dry;
       //prevValue = wet;
    }
    //heads_[WRITE].Write(value);
}

void Looper::UpdateReadPos()
{
    readPos_ = heads_[READ].UpdatePosition();
    readPosSeconds_ = readPos_ / sampleRate_;
}

void Looper::UpdateWritePos()
{
    heads_[WRITE].UpdatePosition();
    writePos_ = heads_[WRITE].GetIntPosition();
}

void Looper::ToggleDirection()
{
    direction_ = heads_[READ].ToggleDirection();
    crossPointFound_ = false;
    mustFade_ = false;
}

void Looper::ToggleReading()
{
    readingActive_ = heads_[READ].ToggleRun();
}

void Looper::ToggleWriting()
{
    writingActive_ = heads_[WRITE].ToggleRun();
}

/*
void Looper::CalculateFadeSamples(int32_t pos)
{
    if (loopLength_ < kSamplesToFade)
    {
        fadeSamples_ = 0;
    }

    else if (IsGoingForward() && pos + kSamplesToFade > loopEnd_)
    {
        fadeSamples_ = loopEnd_ - static_cast<int>(pos);
    }

    else if (IsGoingForward() && pos - kSamplesToFade < loopStart_)
    {
        fadeSamples_ = static_cast<int>(pos);
    }

    else
    {
        fadeSamples_ = kSamplesToFade;
    }
}
*/

void Looper::CalculateHeadsDistance()
{
    // This is the floor of the float position.
    int32_t intReadPos = heads_[READ].GetIntPosition();

    // forward, rp > wp, ws > rs = rp - wp
    // backwards, rp > wp, ws > rs = rp - wp
    // backwards, rp > wp, rs > ws = rp - wp
    if (intReadPos > writePos_ && ((IsGoingForward() && writeSpeed_ > readSpeed_) || (!IsGoingForward() && intReadPos > writePos_)))
    {
        headsDistance_ = intReadPos - writePos_;
    }

    // forward, rp > wp, rs > ws = b - rp + wp
    else if (IsGoingForward() && intReadPos > writePos_)
    {
        headsDistance_ = bufferSamples_ - intReadPos + writePos_;
    }

    // forward, wp > rp, ws > rs = b - wp + rp
    // backwards, wp > rp, ws > rs = b - wp + rp
    // backwards, wp > rp, rs > ws = b - wp + rp
    else if ((IsGoingForward() && writePos_ > intReadPos && writeSpeed_ > readSpeed_) || (!IsGoingForward() && writePos_ > intReadPos))
    {
        headsDistance_ = bufferSamples_ - writePos_ + intReadPos;
    }

    // forward, wp > rp, rs > ws = wp - rp
    else if (IsGoingForward() && readSpeed_ > writeSpeed_)
    {
        headsDistance_ = writePos_ - intReadPos;
    }

    else
    {
        headsDistance_ = 0;
    }
}

void Looper::CalculateCrossPoint()
{
    float relSpeed{writeSpeed_ > readSpeed_ ? writeSpeed_ - readSpeed_ : readSpeed_ - writeSpeed_};
    if (!IsGoingForward())
    {
        relSpeed = writeSpeed_ + readSpeed_;
    }

    float deltaTime = headsDistance_ / relSpeed;
    crossPoint_ = writePos_ + (writeSpeed_ * deltaTime) - 2 * direction_;
    // Wrap the crossing point if it's outside of the buffer.
    if (crossPoint_ >= bufferSamples_)
    {
        int32_t r = crossPoint_ % loopLength_;
        if (loopStart_ + r > bufferSamples_)
        {
            crossPoint_ = r - (bufferSamples_ - loopStart_);
        }
        else
        {
            crossPoint_ = loopStart_ + r;
        }
    }
    crossPointFound_ = true;
}

bool Looper::HandleFade()
{
    if (readingActive_ && writingActive_)
    {
        if (readSpeed_ != writeSpeed_ || !IsGoingForward())
        {
            CalculateHeadsDistance();
            if (!mustFade_)
            {
                // Handle reaching the cross point.
                if (crossPointFound_ && writePos_ == crossPoint_)
                {
                    crossPointFound_ = false;
                    mustFade_ = true;
                    fadeIndex_ = 0;
                    //heads_[READ].Fade();

                    return true;
                }
                // Calculate the cross point.
                else if (!crossPointFound_ && headsDistance_ <= kSamplesToFade)
                {
                    CalculateCrossPoint();
                }
            }
        }
        else if (writePos_ == 0)
        {
            mustFade_ = true;
            fadeIndex_ = 0;

            return true;
        }
    }

    return false;
}

/**
 * @brief Returns a random position within the loop.
 *
 * @return int32_t
 */
int32_t Looper::GetRandomPosition()
{
    int32_t pos{loopStart_ + rand() % (loopLength_ - 1)};
    if (IsGoingForward() && pos > loopEnd_)
    {
        pos = loopEnd_;
    }
    else if (!IsGoingForward() && pos < loopStart_)
    {
        pos = loopStart_;
    }

    return pos;
}