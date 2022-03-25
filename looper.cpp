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
    std::srand(static_cast<unsigned>(time(0)));
    eRand_ = std::rand() / (float)RAND_MAX;
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
    writePos_ = 0.f;
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
    triggerFade.Init(Fader::FadeType::FADE_OUT_IN, heads_[READ].GetSamplesToFade() * 2, readRate_);
}

void Looper::SetSamplesToFade(float samples)
{
    heads_[READ].SetSamplesToFade(samples);
    heads_[WRITE].SetSamplesToFade(samples);
}

void Looper::SetLoopStart(float start)
{
    loopLengthGrown_ = start > loopStart_;

    // Set up a fade when the start point changed, except if we're in delay mode
    // or totally frozen.
    if (loopLength_ < bufferSamples_ && loopLength_ > kMinSamplesForFlanger && freeze_ < 1.f)
    {
        loopLengthChanged_ = true;
        lengthFadePos_ = loopEnd_;
        float samples = std::min(heads_[READ].GetSamplesToFade() * readRate_, (loopLength_ / 2.f) * readRate_);
        // If there's already a fade in progress, reset it.
        if (loopLengthFade_)
        {
            loopLengthFade_ = false;
            loopLengthFade.Reset(samples, readRate_);
        }
        // Otherwise, init a new fade.
        else
        {
            loopLengthFade.Init(Fader::FadeType::FADE_SINGLE, samples, readRate_);
        }
    }

    loopStart_ = heads_[READ].SetLoopStart(start);
    intLoopStart_ = loopStart_;
    heads_[WRITE].SetLoopStart(loopSync_ ? loopStart_ : 0);
    loopStartSeconds_ = loopStart_ / static_cast<float>(sampleRate_);
    loopEnd_ = heads_[READ].GetLoopEnd();
    intLoopEnd_ = loopEnd_;
    crossPointFound_ = false;
    mustSync_ = true;

    // If the reading head goes outside of the loop, reset its position.
    if (loopLength_ < kMinSamplesForFlanger && ((loopEnd_ > loopStart_ && (readPos_ > loopEnd_ || readPos_ < loopStart_)) || (loopStart_ > loopEnd_ && readPos_ > loopEnd_ && readPos_ < loopStart_)))
    {
        heads_[READ].ResetPosition();
        if (loopSync_)
        {
            heads_[WRITE].ResetPosition();
        }
    }
}

void Looper::SetLoopLength(float length)
{
    loopLengthGrown_ = length > loopLength_;

    // Set up a fade when the length changed, except if we're in delay mode or
    // totally frozen.
    if (length < bufferSamples_ && length > kMinSamplesForFlanger && freeze_ < 1.f)
    {
        loopLengthChanged_ = true;
        lengthFadePos_ = loopEnd_;
        float samples = std::min(heads_[READ].GetSamplesToFade(), (length / 2.f) * readRate_);
        // If there's already a fade in progress, reset it.
        if (loopLengthFade_)
        {
            loopLengthFade_ = false;
            loopLengthFade.Reset(samples, readRate_);
        }
        // Otherwise, init a new fade.
        else
        {
            loopLengthFade.Init(Fader::FadeType::FADE_SINGLE, samples, readRate_);
        }
    }

    // Apply the changes.
    loopLength_ = heads_[READ].SetLoopLength(length);
    intLoopLength_ = loopLength_;
    heads_[WRITE].SetLoopLength(loopSync_ ? loopLength_ : bufferSamples_);
    loopLengthSeconds_ = loopLength_ / sampleRate_;
    loopEnd_ = heads_[READ].GetLoopEnd();
    intLoopEnd_ = loopEnd_;
    crossPointFound_ = false;
    mustSync_ = true;

    // If the reading head goes outside of the loop, reset its position.
    if (loopLength_ < kMinSamplesForFlanger && ((loopEnd_ > loopStart_ && (readPos_ > loopEnd_ || readPos_ < loopStart_)) || (loopStart_ > loopEnd_ && readPos_ > loopEnd_ && readPos_ < loopStart_)))
    {
        heads_[READ].ResetPosition();
        if (loopSync_)
        {
            heads_[WRITE].ResetPosition();
        }
    }
}

void Looper::SetReadRate(float rate)
{
    heads_[READ].SetRate(rate);
    readRate_ = rate;
    readSpeed_ = sampleRate_ * readRate_;
    sampleRateSpeed_ = static_cast<int32_t>(sampleRate_ / readRate_);
    crossPointFound_ = false;
    mustSync_ = true;
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
    mustSync_ = true;
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
    if (freeze_ > 0)
    {
        value = Fader::EqualCrossFade(value, heads_[READ].ReadFrozen(), freeze_);
    }

    // If the loop length grew, blend a few samples from the start with those at
    // the old loop end, otherwise, blend the cropped part at the end with the
    // samples at the start.
    if (loopLengthFade_)
    {
        float valueToBlend = heads_[READ].ReadBufferAt((loopLengthGrown_ ? loopStart_ : loopEnd_ + 1) + loopLengthFade.GetIndex());
        if (Fader::FadeStatus::ENDED == loopLengthFade.Process(valueToBlend, value))
        {
            loopLengthFade_ = false;
        }
        value = loopLengthFade.GetOutput();
    }

    // Handle fade when looping.
    if (loopFade.IsActive())
    {
        //float valueToBlend = heads_[READ].ReadBufferAt((Direction::FORWARD == direction_ ? loopStart_ : loopEnd_ + 1) + loopFade.GetIndex());
        loopFade.Process(value, 0);
        value = loopFade.GetOutput();
    }

    // Handle fade on re-triggering.
    if (triggerFade.IsActive())
    {
        if (Fader::FadeStatus::ENDED == triggerFade.Process(value, 0))
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
            heads_[READ].SetRunStatus(RunStatus::RUNNING);
        }
        value = triggerFade.GetOutput();
    }

    return value;
}

void Looper::Write(float input)
{
    if (freeze_ < 1.f && crossPointFade_)
    {
        float currentValue = heads_[WRITE].GetCurrentValue();
        if (Fader::FadeStatus::ENDED == headsCrossFade.Process(input, currentValue))
        {
            crossPointFade_ = false;
        }
        input = headsCrossFade.GetOutput();
    }

    heads_[WRITE].Write(input);
}

float Looper::Degrade(float input)
{
    if (degradation_ > 0.f)
    {
        float d = 1.f - ((std::rand() / (float)RAND_MAX) * degradation_);

        return heads_[WRITE].BresenhamEuclidean(eRand_ * 64, degradation_) ? input : input * d;
    }

    return input;
}

bool Looper::UpdateReadPos()
{
    bool toggle = heads_[READ].UpdatePosition();
    readPos_ = heads_[READ].GetPosition();
    readPosSeconds_ = readPos_ / sampleRate_;

    if (loopLengthChanged_)
    {
        // If the loop length has grown, wait until we reach the position from
        // which we must fade.
        if (loopLengthGrown_)
        {
            float dist = CalculateDistance(readPos_, lengthFadePos_, readRate_, 0, direction_);
            if (dist <= std::numeric_limits<float>::epsilon())
            {
                loopLengthChanged_ = false;
                loopLengthFade_ = true;
            }
        }
        // Otherwise, initiate the fade when the reading head actually loops.
        else if (toggle)
        {
            loopLengthChanged_ = false;
            loopLengthFade_ = true;
        }
    }

    // In looper mode, set a full fade (out + in) when the reading head get near
    // the loop end or loop start, depending on the direction.
    if (!loopSync_ && (loopLength_ < bufferSamples_ || Direction::BACKWARDS == direction_))
    {
        float pos = Direction::FORWARD == direction_ ? loopEnd_ : loopStart_;
        float dist = CalculateDistance(readPos_, pos, readRate_, 0, direction_);
        if (dist <= heads_[READ].GetSamplesToFade())
        {
            loopFade.Init(Fader::FadeType::FADE_OUT_IN, dist * 2, readRate_);
        }
    }

    return toggle;
}

bool Looper::UpdateWritePos()
{
    bool toggle = heads_[WRITE].UpdatePosition();
    writePos_ = heads_[WRITE].GetIntPosition();

    // In delay mode, keep in sync the reading and the writing heads' position
    // each time the latter reaches either the start or the end of the loop
    // (depending on the reading direction).
    if (toggle && loopSync_ && mustSync_)
    {
        heads_[READ].SetIndex(writePos_);
        mustSync_ = false;
    }

    // Handle when reading and writing speeds differ or we're going
    // backwards.
    if (freeze_ < 1.f && !crossPointFade_ && (readSpeed_ != writeSpeed_ || !IsGoingForward()))
    {
        headsDistance_ = CalculateDistance(readPos_, writePos_, readSpeed_, writeSpeed_, direction_);

        // Calculate the cross point when the two heads are close enough.
        if (!crossPointFound_ && headsDistance_ > 0 && headsDistance_ <= heads_[WRITE].GetSamplesToFade() * 2)
        {
            CalculateCrossPoint();
        }

        if (crossPointFound_)
        {
            float samples = CalculateDistance(writePos_, crossPoint_, writeSpeed_, 0, Direction::FORWARD);
            // If the condition are met, set up the cross point fade.
            if (samples > 0 && samples <= heads_[WRITE].GetSamplesToFade())
            {
                crossPointFound_ = false;
                crossPointFade_ = true;
                headsCrossFade.Init(Fader::FadeType::FADE_OUT_IN, samples * 2, writeRate_);
            }
        }
    }

    return toggle;
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

void Looper::SetDegradation(float amount)
{
    degradation_ = amount;
    heads_[READ].SetDegradation(amount);
    heads_[WRITE].SetDegradation(amount);
}

float Looper::CalculateDistance(float a, float b, float aSpeed, float bSpeed, Direction direction)
{
    if (a == b)
    {
        return 0;
    }

    if (loopStart_ > loopEnd_)
    {
        // Broken loop, case where a is in the second segment and b in the first.
        if (a >= loopStart_ && b <= loopEnd_)
        {
            return (Direction::FORWARD == direction && aSpeed > bSpeed) ? loopLength_ - ((loopEnd_ - b) + (a - loopStart_)) : (loopEnd_ - b) + (a - loopStart_);
        }

        // Broken loop, case where b is in the second segment and a in the first.
        if (b >= loopStart_ && a <= loopEnd_)
        {
            return (Direction::BACKWARDS == direction || bSpeed > aSpeed) ? loopLength_ - ((loopEnd_ - a) + (b - loopStart_)) : (loopEnd_ - a) + (b - loopStart_);
        }
    }

    if (a > b)
    {
        return (Direction::FORWARD == direction && aSpeed > bSpeed) ? loopLength_ - (a - b) : a - b;
    }

    return (Direction::BACKWARDS == direction || bSpeed > aSpeed) ? loopLength_ - (b - a) : b - a;
}

void Looper::CalculateCrossPoint()
{
    // Do not calculate the cross point if the write head is outside of
    // the loop (this is especially true in looper mode, when it roams
    // along all the buffer).
    if ((loopEnd_ > loopStart_ && (writePos_ < loopStart_ || writePos_ > loopEnd_)) || (loopStart_ > loopEnd_ && writePos_ < loopStart_ && writePos_ > loopEnd_))
    {
        return;
    }

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
        // Wrap the crossing point if it's outside of the loop.
        if (crossPoint_ > loopEnd_)
        {
            crossPoint_ = loopStart_ + std::fmod(crossPoint_, loopLength_);
        }
        else if (crossPoint_ < loopStart_)
        {
            crossPoint_ = loopStart_ + std::fmod(crossPoint_ + loopStart_, loopLength_);
        }
    }
    // Inverted loop
    else
    {
        // Wrap the crossing point if it's outside of the buffer.
        if (crossPoint_ >= bufferSamples_)
        {
            crossPoint_ = std::fmod(crossPoint_, loopLength_);
        }
        // If the cross point falls just between the loop's start and end point,
        // nudge it forward.
        if (crossPoint_ > loopEnd_ && crossPoint_ < loopStart_)
        {
            crossPoint_ = loopStart_ + (crossPoint_ - loopEnd_);
        }
    }

    crossPoint_ = std::floor(crossPoint_);

    crossPointFound_ = true;
}