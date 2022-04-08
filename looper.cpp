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
    readHeads_[0].Init(buffer, buffer2, maxBufferSamples);
    readHeads_[0].SetActive(true);
    readHeads_[1].Init(buffer, buffer2, maxBufferSamples);
    writeHead_.Init(buffer, buffer2, maxBufferSamples);
    Reset();
    movement_ = Movement::NORMAL;
    direction_ = Direction::FORWARD;
    SetReadRate(1.f);
    SetWriteRate(1.f);
    writeHead_.SetActive(true);
    writeHead_.SetLooping(true);
    writeHead_.SetRunStatus(RunStatus::RUNNING);
}

void Looper::Reset()
{
    std::srand(static_cast<unsigned>(time(0)));
    eRand_ = std::rand() / (float)RAND_MAX;
    readHeads_[0].Reset();
    readHeads_[1].Reset();
    writeHead_.Reset();
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
    writeHead_.ClearBuffer();
}

bool Looper::Buffer(float value)
{
    bool end = writeHead_.Buffer(value);
    bufferSamples_ = writeHead_.GetBufferSamples();
    bufferSeconds_ = bufferSamples_ / static_cast<float>(sampleRate_);

    return end;
}

void Looper::StopBuffering()
{
    float samples = writeHead_.StopBuffering();
    readHeads_[0].InitBuffer(samples);
    readHeads_[1].InitBuffer(samples);
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
        readHeads_[activeReadHead_].SetRunStatus(RunStatus::RUNNING);

        return true;
    }

    RunStatus status = readHeads_[activeReadHead_].GetRunStatus();
    if (RunStatus::RUNNING != status && RunStatus::STARTING != status)
    {
        status = readHeads_[activeReadHead_].Start();
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
        readHeads_[activeReadHead_].SetRunStatus(RunStatus::STOPPED);

        return true;
    }

    RunStatus status = readHeads_[activeReadHead_].GetRunStatus();
    if (RunStatus::STOPPED != status && RunStatus::STOPPING != status)
    {
        status = readHeads_[activeReadHead_].Stop();
    }
    if (RunStatus::STOPPED == status)
    {
        return true;
    }

    return false;
}

void Looper::Trigger()
{
    triggerFade.Init(Fader::FadeType::FADE_OUT_IN, readHeads_[activeReadHead_].GetSamplesToFade() * 2, readRate_);
}

void Looper::SetSamplesToFade(float samples)
{
    readHeads_[0].SetSamplesToFade(samples);
    readHeads_[1].SetSamplesToFade(samples);
    writeHead_.SetSamplesToFade(samples);
}

void Looper::SetLoopStart(float start)
{
    loopLengthGrown_ = start > loopStart_;
    loopStart_ = readHeads_[0].SetLoopStart(start);
    readHeads_[1].SetLoopStart(loopStart_);
    intLoopStart_ = loopStart_;
    writeHead_.SetLoopStart(loopSync_ ? loopStart_ : 0);
    loopStartSeconds_ = loopStart_ / static_cast<float>(sampleRate_);
    loopEnd_ = readHeads_[activeReadHead_].GetLoopEnd();
    intLoopEnd_ = loopEnd_;
    crossPointFound_ = false;
    mustSyncHeads_ = true;
}

void Looper::SetLoopLength(float length)
{
    loopLengthGrown_ = length > loopLength_;
    loopLengthReset_ = length == bufferSamples_;
    loopLength_ = readHeads_[!activeReadHead_].SetLoopLength(length);
    // Only set the active reading head's loop length right away either when in
    // delay mode or when the length shrunk. When it grows, we need this head's
    // end position for triggering the loop fade.
    if (!loopLengthGrown_ || loopSync_)
    {
        readHeads_[activeReadHead_].SetLoopLength(length);
    }
    intLoopLength_ = loopLength_;
    writeHead_.SetLoopLength(loopSync_ ? loopLength_ : bufferSamples_);
    loopLengthSeconds_ = loopLength_ / sampleRate_;
    loopEnd_ = readHeads_[!activeReadHead_].GetLoopEnd();
    intLoopEnd_ = loopEnd_;
    crossPointFound_ = false;
    mustSyncHeads_ = true;
}

void Looper::SetReadRate(float rate)
{
    readHeads_[0].SetRate(rate);
    readHeads_[1].SetRate(rate);
    readRate_ = rate;
    readSpeed_ = sampleRate_ * readRate_;
    sampleRateSpeed_ = static_cast<int32_t>(sampleRate_ / readRate_);
    crossPointFound_ = false;
    mustSyncHeads_ = true;
}

void Looper::SetWriteRate(float rate)
{
    writeHead_.SetRate(rate);
    writeRate_ = rate;
    writeSpeed_ = sampleRate_ * writeRate_;
    crossPointFound_ = false;
}

void Looper::SetMovement(Movement movement)
{
    readHeads_[0].SetMovement(movement);
    readHeads_[1].SetMovement(movement);
    movement_ = movement;
}

void Looper::SetDirection(Direction direction)
{
    readHeads_[0].SetDirection(direction);
    readHeads_[1].SetDirection(direction);
    direction_ = direction;
    crossPointFound_ = false;
    mustSyncHeads_ = true;
}

void Looper::SetReadPos(float position)
{
    readHeads_[0].SetIndex(position);
    readHeads_[1].SetIndex(position);
    readPos_ = position;
    crossPointFound_ = false;
}

void Looper::SetWritePos(float position)
{
    writeHead_.SetIndex(position);
    writePos_ = position;
    crossPointFound_ = false;
}

void Looper::SetLooping(bool looping)
{
    readHeads_[0].SetLooping(looping);
    readHeads_[1].SetLooping(looping);
    looping_ = looping;
    crossPointFound_ = false;
}

void Looper::SetLoopSync(bool loopSync)
{
    if (loopSync_ && !loopSync)
    {
        writeHead_.SetLoopLength(bufferSamples_);
    }
    else if (!loopSync_ && loopSync)
    {
        writeHead_.SetLoopLength(readHeads_[activeReadHead_].GetLoopLength());
    }
    loopSync_ = loopSync;
    readHeads_[0].SetLoopSync(loopSync_);
    readHeads_[1].SetLoopSync(loopSync_);
    writeHead_.SetLoopSync(loopSync_);

    crossPointFound_ = false;
}

float Looper::Read(float input)
{
    float value = readHeads_[activeReadHead_].Read(input);
    if (loopFade.IsActive())
    {
        if (Fader::FadeStatus::ENDED == loopFade.Process(readHeads_[!activeReadHead_].Read(input), value))
        {
            // FIXME: This sometimes causes the looper to fall silent, not sure
            // if it's even needed at this point...
            //readHeads_[!activeReadHead_].SetRunStatus(RunStatus::STOPPED);
            readHeads_[!activeReadHead_].SetLoopLength(loopLength_);
        }
        else
        {
            value = loopFade.GetOutput();
        }
    }

    if (freeze_ > 0)
    {
        value = Fader::EqualCrossFade(value, readHeads_[activeReadHead_].ReadFrozen(), freeze_);
    }

    // Handle fade on re-triggering.
    if (triggerFade.IsActive())
    {
        if (Fader::FadeStatus::ENDED == triggerFade.Process(value, 0))
        {
            // Invert direction when in pendulum or drunk mode.
            if (Movement::PENDULUM == movement_ || Movement::DRUNK == movement_)
            {
                direction_ = readHeads_[activeReadHead_].ToggleDirection();
            }
            readHeads_[activeReadHead_].ResetPosition();
            if (looping_ || loopSync_)
            {
                writeHead_.ResetPosition();
            }
            readHeads_[activeReadHead_].SetRunStatus(RunStatus::RUNNING);
        }
        else
        {
            value = triggerFade.GetOutput();
        }
    }

    return value;
}

void Looper::Write(float input)
{
    if (freeze_ < 1.f && headsCrossFade.IsActive())
    {
        float currentValue = writeHead_.GetCurrentValue();
        headsCrossFade.Process(input, currentValue);
        input = headsCrossFade.GetOutput();
    }

    writeHead_.Write(input);
}

float Looper::Degrade(float input)
{
    if (degradation_ > 0.f)
    {
        float d = 1.f - ((std::rand() / (float)RAND_MAX) * degradation_) * 0.5f;

        return writeHead_.BresenhamEuclidean(eRand_ * 64, degradation_) ? input : input * d;
    }

    return input;
}

bool Looper::UpdateReadPos()
{
    bool triggered = readHeads_[activeReadHead_].UpdatePosition();
    readHeads_[!activeReadHead_].UpdatePosition();

    // When looping, the active reading head should fade out and proceed reading
    // ignoring the loop boundaries, while the inactive reading head should begin
    // a fade in from the other side of the loop.
    if (triggered && !loopSync_ && (loopLengthReset_ || loopLength_ < bufferSamples_ || Direction::BACKWARDS == direction_))
    {
        SwitchReadingHeads();
        loopLengthReset_ = false;
    }

    readPos_ = readHeads_[activeReadHead_].GetPosition();
    readPosSeconds_ = readPos_ / sampleRate_;

    return triggered;
}

bool Looper::UpdateWritePos()
{
    bool triggered = writeHead_.UpdatePosition();
    writePos_ = writeHead_.GetIntPosition();

    // In delay mode, keep in sync the reading and the writing heads' position
    // each time the latter reaches either the start or the end of the loop
    // (depending on the reading direction).
    if (triggered && loopSync_ && mustSyncHeads_ && readRate_ == 1.f)
    {
        //readHeads_[activeReadHead_].SetIndex(writePos_);
        mustSyncHeads_ = false;
    }

    // Handle when reading and writing speeds differ or we're going
    // backwards.
    if (freeze_ < 1.f && !headsCrossFade.IsActive() && (readSpeed_ != writeSpeed_ || !IsGoingForward()))
    {
        headsDistance_ = CalculateDistance(readPos_, writePos_, readSpeed_, writeSpeed_, direction_);

        // Calculate the cross point when the two heads are close enough.
        if (!crossPointFound_ && headsDistance_ > 0 && headsDistance_ <= writeHead_.GetSamplesToFade() * 2)
        {
            CalculateCrossPoint();
        }

        if (crossPointFound_)
        {
            float samples = CalculateDistance(writePos_, crossPoint_, writeSpeed_, 0, Direction::FORWARD);
            // If the condition are met, set up the cross point fade.
            if (samples > 0 && samples <= writeHead_.GetSamplesToFade())
            {
                crossPointFound_ = false;
                headsCrossFade.Init(Fader::FadeType::FADE_OUT_IN, samples * 2, writeRate_);
            }
        }
    }

    return triggered;
}

void Looper::SwitchReadingHeads()
{
    if (loopFade.IsActive())
    {
        return;
    }

    // Swap the heads: the active one will fade out and the inactive one will
    // fade in at the start position.

    readHeads_[activeReadHead_].SetActive(false);
    readHeads_[!activeReadHead_].SetRunStatus(RunStatus::RUNNING);
    readHeads_[!activeReadHead_].SetActive(true);
    if (loopLengthGrown_)
    {
        readHeads_[activeReadHead_].ResetPosition();
    }
    else
    {
        readHeads_[!activeReadHead_].ResetPosition();
    }

    float samples = std::min(readHeads_[activeReadHead_].GetSamplesToFade(), readHeads_[!activeReadHead_].GetSamplesToFade());
    loopFade.Init(Fader::FadeType::FADE_SINGLE, samples, readRate_);
    activeReadHead_ = !activeReadHead_;
}

void Looper::ToggleDirection()
{
    direction_ = readHeads_[0].ToggleDirection();
    readHeads_[1].ToggleDirection();
}

void Looper::SetFreeze(float amount)
{
    freeze_ = amount;
    readHeads_[0].SetFreeze(amount);
    readHeads_[1].SetFreeze(amount);
    writeHead_.SetFreeze(amount);
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
    readHeads_[0].SetDegradation(amount);
    readHeads_[1].SetDegradation(amount);
    writeHead_.SetDegradation(amount);
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