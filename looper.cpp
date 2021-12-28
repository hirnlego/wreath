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
    SetRate(1.f);
}

void Looper::ClearBuffer()
{
    heads_[WRITE].ClearBuffer();
}

bool Looper::Buffer(float value)
{
    bool end = heads_[WRITE].Buffer(value);
    bufferSamples_ = heads_[WRITE].GetBufferSamples();
    bufferSeconds_ = bufferSamples_ / static_cast<float>(sampleRate_);

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
}

void Looper::SetRate(float rate)
{
    heads_[READ].SetRate(rate);
    rate_ = rate;
    readSpeed_ = sampleRate_ * rate_; // samples/s.
    writeSpeed_ = sampleRate_;             // samples/s.
    sampleRateSpeed_ = static_cast<int32_t>(sampleRate_ / rate_);
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

float Looper::Read()
{
    float val = heads_[READ].Read();

    // Fade the value if needed.
    if (mustFade_ != -1)
    {
        fadeIndex_++;
        if (fadeIndex_ > fadeSamples_)
        {
            crossPointFound_ = false;
            mustFade_ = Fade::NONE;
        }
        /*
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
                mustFade_ = Fade::IN;
                val = 0.f;
            }
            else
            {
                crossPointFound_ = false;
                mustFade_ = Fade::NONE;
            }
        }
        */
    }

    return val;
}

void Looper::Write(float value)
{
    heads_[WRITE].Write(value);
}

void Looper::UpdateReadPos()
{
    readPos_ = heads_[READ].UpdatePosition();
    readPosSeconds_ = readPos_ / sampleRate_;

    if (readingActive_ && writingActive_)
    {
        if (readSpeed_ != writeSpeed_ || !IsGoingForward())
        {
            CalculateHeadsDistance();
        }
        if (mustFade_ == Fade::NONE)
        {
            CalculateFadeSamples(static_cast<int32_t>(std::round(readPos_)));
            if (fadeSamples_ > 0)
            {
                HandleFade();
            }
        }
    }
}

void Looper::UpdateWritePos()
{
    writePos_ = heads_[WRITE].UpdatePosition();
}

void Looper::ToggleDirection()
{
    direction_ = heads_[READ].ToggleDirection();
}

void Looper::ToggleReading()
{
    readingActive_ = heads_[READ].ToggleRun();
}

void Looper::ToggleWriting()
{
    writingActive_ = heads_[WRITE].ToggleRun();
}

void Looper::CalculateFadeSamples(int32_t pos)
{
    fadeSamples_ = kSamplesToFade;

    /* TODO?
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
    */
}

void Looper::CalculateHeadsDistance()
{
    // forward, rp > wp, ws > rs = rp - wp
    // backwards, rp > wp, ws > rs = rp - wp
    // backwards, rp > wp, rs > ws = rp - wp
    if (readPos_ > writePos_ && ((IsGoingForward() && writeSpeed_ > readSpeed_) || (!IsGoingForward() && readPos_ > writePos_)))
    {
        headsDistance_ = readPos_ - writePos_;
    }

    // forward, rp > wp, rs > ws = b - rp + wp
    else if (IsGoingForward() && readPos_ > writePos_)
    {
        headsDistance_ = bufferSamples_ - readPos_ + writePos_;
    }

    // forward, wp > rp, ws > rs = b - wp + rp
    // backwards, wp > rp, ws > rs = b - wp + rp
    // backwards, wp > rp, rs > ws = b - wp + rp
    else if ((IsGoingForward() && writePos_ > readPos_ && writeSpeed_ > readSpeed_) || (!IsGoingForward() && writePos_ > readPos_))
    {
        headsDistance_ = bufferSamples_ - writePos_ + readPos_;
    }

    // forward, wp > rp, rs > ws = wp - rp
    else if (IsGoingForward() && readSpeed_ > writeSpeed_)
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
    if ((readSpeed_ != writeSpeed_ || !IsGoingForward()) && headsDistance_ > 0 && headsDistance_ <= fadeSamples_ * 2 && !crossPointFound_ && writingActive_)
    {
        float deltaTime{};
        float relSpeed{writeSpeed_ > readSpeed_ ? writeSpeed_ - readSpeed_ : readSpeed_ - writeSpeed_};
        if (!IsGoingForward())
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
    if ((IsGoingForward() && writePos_ == loopStart_) ||
        (!IsGoingForward() && writePos_ == loopEnd_))
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
        heads_[READ].Fade(fadeSamples_);
        fadeIndex_ = 0;
        mustFade_ = Fade::OUT;
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