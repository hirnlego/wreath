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
void Looper::Init(int32_t sampleRate, float *buffer, float *buffer2, int32_t maxBufferSamples)
{
    sampleRate_ = sampleRate;
    heads_[READ].Init(buffer, buffer2, maxBufferSamples);
    heads_[WRITE].Init(buffer, buffer2, maxBufferSamples);
    Reset();
    movement_ = Movement::NORMAL;
    direction_ = Direction::FORWARD;
    SetReadRate(1.f);
    SetWriteRate(1.f);
    heads_[WRITE].SetLooping(true);
    heads_[WRITE].SetRunStatus(RunStatus::RUNNING);
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
    loopLength_ = 0.f;
    intLoopEnd_ = 0;
    intLoopLength_ = 0;
    loopLengthSeconds_ = 0.f;
    readPos_ = 0.f;
    readPosSeconds_ = 0.f;
    writePos_ = 0;
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
    loopEnd_ = bufferSamples_ - 1;
    intLoopEnd_ = loopEnd_;
    loopLength_ = bufferSamples_;
    intLoopLength_ = bufferSamples_;
    loopLengthSeconds_ = loopLength_ / sampleRate_;
}

bool Looper::Start(bool now)
{
    if (now)
    {
        heads_[READ].SetRunStatus(RunStatus::RUNNING);

        return true;
    }

    RunStatus status = heads_[READ].GetRunStatus();
    if (RunStatus::RUNNING != status && RunStatus::STARTING != status)
    {
        status = heads_[READ].Start();
    }
    if (RunStatus::RUNNING == status)
    {
        return true;
    }

    return false;
}

bool Looper::Stop(bool now)
{
    if (now)
    {
        heads_[READ].SetRunStatus(RunStatus::STOPPED);

        return true;
    }

    RunStatus status = heads_[READ].GetRunStatus();
    if (RunStatus::STOPPED != status && RunStatus::STOPPING != status)
    {
        status = heads_[READ].Stop();
    }
    if (RunStatus::STOPPED == status)
    {
        return true;
    }

    return false;
}

void Looper::Trigger()
{
    // Fade reading.
    heads_[READ].ResetPosition();
    if (loopSync_)
    {
        heads_[WRITE].ResetPosition();
    }
    heads_[READ].SetRunStatus(RunStatus::RUNNING);
    mustFadeRead_ = Fade::FADE_OUT_IN;
    readFadeIndex_ = 0;
}

bool Looper::Restart(bool resetPosition)
{
    RunStatus status = heads_[READ].GetRunStatus();
    if (!isRestarting_)
    {
        isRestarting_ = true;
        Stop(true);
    }
    else if (RunStatus::STOPPED == status)
    {
        if (resetPosition)
        {
            heads_[READ].ResetPosition();
            heads_[WRITE].ResetPosition();
            // Invert direction when in pendulum.
            if (Movement::PENDULUM == movement_ || Movement::DRUNK == movement_)
            {
                direction_ = heads_[READ].ToggleDirection();
            }
        }

        Start(false);
    }
    else if (RunStatus::RUNNING == status)
    {
        isRestarting_ = false;

        return true;
    }

    return false;
}

void Looper::SetSamplesToFade(float samples)
{
    heads_[READ].SetSamplesToFade(samples);
    heads_[WRITE].SetSamplesToFade(samples);
}

void Looper::SetLoopStart(float start)
{
    loopStart_ = heads_[READ].SetLoopStart(start);
    intLoopStart_ = loopStart_;
    heads_[WRITE].SetLoopStart(loopSync_ ? loopStart_ : 0);
    loopStartSeconds_ = loopStart_ / static_cast<float>(sampleRate_);
    loopEnd_ = heads_[READ].GetLoopEnd();
    intLoopEnd_ = loopEnd_;
    crossPointFound_ = false;
};

void Looper::SetLoopEnd(float end)
{
    loopEnd_ = end;
    intLoopEnd_ = loopEnd_;
}

void Looper::SetLoopLength(float length)
{
    loopLength_ = heads_[READ].SetLoopLength(length);
    intLoopLength_ = loopLength_;
    heads_[WRITE].SetLoopLength(loopSync_ ? loopLength_ : bufferSamples_);
    loopLengthSeconds_ = loopLength_ / sampleRate_;
    loopEnd_ = heads_[READ].GetLoopEnd();
    intLoopEnd_ = loopEnd_;

    if (loopLength_ > heads_[READ].GetSamplesToFade())
    {
        if (mustPaste_)
        {
            //heads_[READ].PasteFadeBuffer(fadePos_);
            mustPaste_ = false;
        }

        //fadePos_ = loopEnd_ - heads_[READ].GetSamplesToFade() + 1;
        //heads_[READ].CopyFadeBuffer(fadePos_);
        mustPaste_ = true;
    }

    crossPointFound_ = false;
}

void Looper::SetReadRate(float rate)
{
    heads_[READ].SetRate(rate);
    readRate_ = rate;
    readSpeed_ = sampleRate_ * readRate_;
    sampleRateSpeed_ = static_cast<int32_t>(sampleRate_ / readRate_);
    crossPointFound_ = false;
}

void Looper::SetWriteRate(float rate)
{
    heads_[WRITE].SetRate(rate);
    writeRate_ = rate;
    writeSpeed_ = sampleRate_ * writeRate_;
    crossPointFound_ = false;
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
    crossPointFound_ = false;
}

void Looper::SetReadPos(float position)
{
    heads_[READ].SetIndex(position);
    readPos_ = position;
    crossPointFound_ = false;
}

void Looper::SetWritePos(float position)
{
    heads_[WRITE].SetIndex(position);
    writePos_ = position;
    crossPointFound_ = false;
}

void Looper::SetLooping(bool looping)
{
    heads_[READ].SetLooping(looping);
    looping_ = looping;
    crossPointFound_ = false;
}

void Looper::SetLoopSync(bool loopSync)
{
    if (loopSync_ && !loopSync)
    {
        heads_[WRITE].SetLoopLength(bufferSamples_);
    }
    else if (!loopSync_ && loopSync)
    {
        heads_[WRITE].SetLoopLength(heads_[READ].GetLoopLength());
    }
    loopSync_ = loopSync;
    crossPointFound_ = false;
}

float Looper::Read(float input)
{
    float value = heads_[READ].Read(input);
    float valueToFade = 0;//freeze_ == 1.f ? 0 : input;
    float samples = heads_[READ].GetSamplesToFade();

    // Set a full fade (out + in) when the read head get near the loop end or
    // loop start, depending on the direction.
    // Note: this is needed in both looper and delay mode!
    int32_t pos = Direction::FORWARD == direction_ ? intLoopEnd_ - (samples * readRate_) : intLoopStart_ + (samples * readRate_);
    if ((intLoopLength_ < bufferSamples_ || Direction::BACKWARDS == direction_) && intLoopLength_ >= samples && heads_[READ].GetIntPosition() == pos && Fade::NO_FADE == mustFadeRead_)
    {
        mustFadeRead_ = Fade::FADE_OUT_IN;
        readFadeIndex_ = 0;
    }

    if (Fade::FADE_IN == mustFadeRead_)
    {
        value = Head::EqualCrossFade(valueToFade, value, readFadeIndex_ * (1.f / samples));
        if (readFadeIndex_ >= samples)
        {
            mustFadeRead_ = Fade::NO_FADE;
        }
        readFadeIndex_ += readRate_;
    }
    else if (Fade::FADE_OUT == mustFadeRead_ || Fade::FADE_OUT_IN == mustFadeRead_ || Fade::FADE_TRIGGER == mustFadeRead_)
    {
        value = Head::EqualCrossFade(value, valueToFade, readFadeIndex_ * (1.f / samples));
        if (readFadeIndex_ >= samples)
        {
            if (Fade::FADE_OUT_IN == mustFadeRead_ || Fade::FADE_TRIGGER == mustFadeRead_)
            {
                if (Fade::FADE_TRIGGER == mustFadeRead_)
                {
                    // Invert direction when in pendulum or drunk mode.
                    if (Movement::PENDULUM == movement_ || Movement::DRUNK == movement_)
                    {
                        direction_ = heads_[READ].ToggleDirection();
                    }
                    heads_[READ].ResetPosition();
                    if (looping_ || loopSync_)
                    {
                        heads_[WRITE].ResetPosition();
                    }
                }
                mustFadeRead_ = Fade::FADE_IN;
                readFadeIndex_ = 0;
            }
            else
            {
                mustFadeRead_ = Fade::NO_FADE;
            }
        }
        readFadeIndex_ += readRate_;
    }

    return value;
}

void Looper::Write(float input)
{
    if (!freeze_)
    {
        float currentValue = heads_[READ].GetCurrentValue();
        currentValue = 0;
        float samples = isFading_ ? mustFadeWriteSamples_ : heads_[WRITE].GetSamplesToFade();
        if (Fade::FADE_IN == mustFadeWrite_)
        {
            input = Head::EqualCrossFade(currentValue, input, writeFadeIndex_ * (1.f / samples));
            if (writeFadeIndex_ >= samples)
            {
                isFading_ = false;
                crossPointFound_ = false;
                mustFadeWrite_ = Fade::NO_FADE;
            }
            writeFadeIndex_ += std::max(1.f, readRate_);
        }
        else if (Fade::FADE_OUT == mustFadeWrite_ || Fade::FADE_OUT_IN == mustFadeWrite_)
        {
            input = Head::EqualCrossFade(input, currentValue, writeFadeIndex_ * (1.f / samples));
            if (writeFadeIndex_ >= samples)
            {
                if (Fade::FADE_OUT_IN == mustFadeWrite_)
                {
                    mustFadeWrite_ = Fade::FADE_IN;
                    writeFadeIndex_ = 0;
                }
                else
                {
                    mustFadeWrite_ = Fade::NO_FADE;
                }
            }
            writeFadeIndex_ += std::max(1.f, readRate_);
        }
    }

    heads_[WRITE].Write(input);
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
    if (loopSync_ && intLoopLength_ <= kMinSamplesForFlanger && freeze_ == 0)
    {
        if ((Direction::FORWARD == direction_ && writePos_ == intLoopStart_) || (Direction::BACKWARDS == direction_ && writePos_ == intLoopEnd_))
        {
            heads_[READ].SetIndex(writePos_);
        }
    }
}

void Looper::ToggleDirection()
{
    direction_ = heads_[READ].ToggleDirection();
}

void Looper::SetFreeze(float amount)
{
    freeze_ = amount;
    heads_[READ].SetFreeze(amount);
    heads_[WRITE].SetFreeze(amount);
}

void Looper::SetTriggerMode(TriggerMode mode)
{
    switch (mode)
    {
    case TriggerMode::GATE:
        SetReadPos(GetWritePos());
        SetLooping(true);
        break;
    case TriggerMode::TRIGGER:
        SetLooping(false);
        break;
    case TriggerMode::LOOP:
        SetLooping(true);
        break;
    }

    triggerMode_ = mode;
}

int32_t Looper::CalculateDistance(int32_t a, int32_t b, float aSpeed, float bSpeed)
{
    if (a == b)
    {
        return 0;
    }

    // Normal loop
    if (loopEnd_ > loopStart_)
    {
        if (a > b)
        {
            return (aSpeed > bSpeed) ? loopEnd_ - a + b : a - b;
        }

        return (bSpeed > aSpeed) ? loopEnd_ - b + a : b - a;
    }

    // Broken loop, case where a is in the second segment and b in the first.
    if (a > loopStart_ && b < loopEnd_)
    {
        return (aSpeed > bSpeed) ? (bufferSamples_ - 1) - a + b : (loopEnd_ - b) + (a - loopStart_);
    }

    // Broken loop, case where b is in the second segment and a in the first.
    if (b > loopStart_ && a < loopEnd_)
    {
        return (bSpeed > aSpeed) ? (bufferSamples_ - 1) - b + a : (loopEnd_ - a) + (b - loopStart_);
    }

    if (a > b)
    {
        return (aSpeed > bSpeed) ? loopLength_ - (a - b) : a - b;
    }

    return (bSpeed > aSpeed) ? loopLength_ - (b - a) : b - a;
}

void Looper::CalculateCrossPoint()
{
    float relSpeed{writeSpeed_ > readSpeed_ ? writeSpeed_ - readSpeed_ : readSpeed_ - writeSpeed_};
    if (!IsGoingForward())
    {
        relSpeed = writeSpeed_ + readSpeed_;
    }

    float deltaTime = headsDistance_ / relSpeed;
    crossPoint_ = writePos_ + writeSpeed_ * deltaTime;

    // Normal loop
    if (loopEnd_ > loopStart_)
    {
        // Wrap the crossing point if it's outside of the loop (or the buffer).
        if (crossPoint_ > loopEnd_ || crossPoint_ >= bufferSamples_)
        {
            int32_t r = crossPoint_ % intLoopLength_;
            if (loopStart_ + r > bufferSamples_)
            {
                crossPoint_ = r - (bufferSamples_ - loopStart_);
            }
            else
            {
                crossPoint_ = loopStart_ + r;
            }
        }
    }
    else
    {
        // Wrap the crossing point if it's outside of the buffer.
        if (crossPoint_ >= bufferSamples_)
        {
            int32_t r = crossPoint_ % intLoopLength_;
            if (loopStart_ + r > bufferSamples_)
            {
                crossPoint_ = r - (bufferSamples_ - loopStart_);
            }
            else
            {
                crossPoint_ = loopStart_ + r;
            }
        }
        // If the cross point falls just between the loop's start and end point,
        // nudge it forward.
        if (crossPoint_ > loopEnd_ && crossPoint_ < loopStart_)
        {
            crossPoint_ = loopStart_ + (crossPoint_ - loopEnd_);
        }
    }

    crossPointFound_ = true;
}

void Looper::HandleFade()
{
    int32_t intReadPos = heads_[READ].GetIntPosition();
/*
    if (isFading_)
    {
        // When the write head stopped we may restart it.
        if (RunStatus::STOPPED == heads_[WRITE].GetRunStatus())
        {
            heads_[WRITE].Start();
        }
        // Finally, remove any trace of the fade operation.
        if (RunStatus::RUNNING == heads_[WRITE].GetRunStatus())
        {
            crossPointFound_ = false;
            isFading_ = false;
        }
    }
*/
    // Fading is not needed when frozen.
    if (freeze_ == 1 || isFading_)
    {
        return;
    }

    // Handle when reading and writing speeds differ or we're going
    // backwards.
    if (readSpeed_ != writeSpeed_ || !IsGoingForward())
    {
        headsDistance_ = CalculateDistance(intReadPos, writePos_, readSpeed_, writeSpeed_);

        // Calculate the cross point.
        if (!crossPointFound_ && headsDistance_ > 0 && headsDistance_ <= heads_[READ].GetSamplesToFade() * 2)
        {
            CalculateCrossPoint();
        }

        if (crossPointFound_ && !isFading_)
        {
            // Calculate the correct point to start the write head's fade,
            // that is the minimum distance from the cross point of the read
            // and write heads.
            // The fade samples and rate are calculated taking into account
            // the heads' speeds and the direction.
            int32_t samples = (writeSpeed_ < readSpeed_) ? CalculateDistance(intReadPos, crossPoint_, readSpeed_, 0) : CalculateDistance(writePos_, crossPoint_, writeSpeed_, 0);
            // If the condition are met, start the fade out of the write head.
            if (samples > 0 && samples <= heads_[READ].GetSamplesToFade())
            {
                //heads_[WRITE].SetWriteFade(samples, std::max(1.f, readRate_));
                //heads_[WRITE].Stop();
                isFading_ = true;
                mustFadeWrite_ = Fade::FADE_OUT_IN;
                mustFadeWriteSamples_ = samples;
                writeFadeIndex_ = 0;
            }
        }
    }
}

/**
 * @brief Returns a random position within the loop.
 *
 * @return int32_t
 */
/*
int32_t Looper::GetRandomPosition()
{
    int32_t pos{loopStart_ + rand() % (intLoopLength_ - 1)};
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
*/