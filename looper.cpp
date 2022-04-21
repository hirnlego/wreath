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
    //readHeads_[0].SetActive(true);
    readHeads_[1].Init(buffer, buffer2, maxBufferSamples);
    writeHead_.Init(buffer, buffer2, maxBufferSamples);
    Reset();
    movement_ = Movement::NORMAL;
    direction_ = Direction::FORWARD;
    readRate_ = 1.f;
    readHeads_[0].SetRate(readRate_);
    readHeads_[1].SetRate(readRate_);
    readSpeed_ = sampleRate_ * readRate_;
    sampleRateSpeed_ = static_cast<int32_t>(sampleRate_ / readRate_);
    readHeads_[0].SetLooping(true);
    readHeads_[1].SetLooping(true);
    writeRate_ = 1.f;
    writeHead_.SetRate(writeRate_);
    writeSpeed_ = sampleRate_ * writeRate_;
    //writeHead_.SetActive(true);
    writeHead_.SetLooping(true);
    //writeHead_.SetRunStatus(RunStatus::RUNNING);
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

void Looper::Start(bool now)
{
    if (now)
    {
        readingActive_ = true;
    }
    else
    {
        startFade.Init(Fader::FadeType::FADE_SINGLE, readHeads_[activeReadHead_].GetSamplesToFade(), readRate_);
    }
}

void Looper::Stop(bool now)
{
    if (now)
    {
        readingActive_ = false;
    }
    else
    {
        stopFade.Init(Fader::FadeType::FADE_SINGLE, readHeads_[activeReadHead_].GetSamplesToFade(), readRate_);
    }
}

void Looper::Trigger()
{
    Start(true);
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
    if (loopFade.IsActive())
    {
        return;
    }

    if (loopLength_ < bufferSamples_)
    {
        loopLengthGrown_ = start > loopStart_;
        loopChanged_ = start != loopStart_;
    }
    loopStart_ = readHeads_[!activeReadHead_].SetLoopStart(start);
    intLoopStart_ = loopStart_;
    if (loopSync_)
    {
        readHeads_[activeReadHead_].SetLoopStart(loopStart_);
        writeHead_.SetLoopStart(loopStart_);
    }
    loopStartSeconds_ = loopStart_ / static_cast<float>(sampleRate_);
    loopEnd_ = readHeads_[!activeReadHead_].GetLoopEnd();
    intLoopEnd_ = loopEnd_;
    crossPointFound_ = false;
}

void Looper::SetLoopLength(float length)
{
    if (loopFade.IsActive())
    {
        return;
    }

    loopLengthGrown_ = length > loopLength_;
    loopChanged_ = length != loopLength_;
    loopLength_ = readHeads_[!activeReadHead_].SetLoopLength(length);
    intLoopLength_ = loopLength_;
    if (loopSync_)
    {
        readHeads_[activeReadHead_].SetLoopLength(loopLength_);
        writeHead_.SetLoopLength(loopLength_);
    }
    loopLengthSeconds_ = loopLength_ / sampleRate_;
    loopEnd_ = readHeads_[!activeReadHead_].GetLoopEnd();
    intLoopEnd_ = loopEnd_;
    crossPointFound_ = false;
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
    float value = readHeads_[activeReadHead_].Read();

    if (startFade.IsActive())
    {
        if (Fader::FadeStatus::ENDED == startFade.Process(0, value))
        {
            readingActive_ = true;
        }
        value = startFade.GetOutput();
    }
    else if (stopFade.IsActive())
    {
        if (Fader::FadeStatus::ENDED == stopFade.Process(value, 0))
        {
            readingActive_ = false;
        }
        value = stopFade.GetOutput();
    }
    else if (!readingActive_)
    {
        return 0.f;
    }

    if (loopFade.IsActive())
    {
        if (Fader::FadeStatus::ENDED == loopFade.Process(readHeads_[!activeReadHead_].Read(), value))
        {
            //readHeads_[!activeReadHead_].SetRunStatus(RunStatus::STOPPED);
            readHeads_[!activeReadHead_].SetLoopStartAndLength(loopStart_, loopLength_);
            readHeads_[!activeReadHead_].SetIndex(readPos_);
        }
        value = loopFade.GetOutput();
    }

    if (freeze_ > 0)
    {
        // Crossfade with the frozen buffer.
        value = Fader::EqualCrossFade(value, readHeads_[activeReadHead_].ReadFrozen(), freeze_);
    }

    /*
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
            //readHeads_[activeReadHead_].SetRunStatus(RunStatus::RUNNING);
        }
        value = triggerFade.GetOutput();
    }
    */

    return value;
}

void Looper::Write(float input)
{
    if (!writingActive_)
    {
        return;
    }

    if (freeze_ < 1.f && headsCrossFade.IsActive())
    {
        headsCrossFade.Process(input, writeHead_.Read());
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

void Looper::SwitchReadingHeads()
{
    if (loopFade.IsActive())
    {
        return;
    }

    // Keep synchronized the writing head in delay mode.
    if (loopSync_ && loopChanged_)
    {
        //writeHead_.SetLoopStartAndLength(loopStart_, loopLength_);
        if (!loopLengthGrown_)
        {
            //writeHead_.ResetPosition();
        }
        if (loopLengthGrown_)
        {
            loopLengthFade_ = true;
        }
    }

    // Swap the heads: the active one will fade out and the inactive one will
    // fade in at the start position.

    //readHeads_[!activeReadHead_].SetRunStatus(RunStatus::RUNNING);
    if (loopLengthGrown_ )
    {
        readHeads_[activeReadHead_].ResetPosition();
        loopLengthGrown_ = false;
    }
    else
    {
        readHeads_[!activeReadHead_].ResetPosition();
    }

    // Active: length - buffer
    // inactive: length
    float samples = std::min(readHeads_[activeReadHead_].GetSamplesToFade(), readHeads_[!activeReadHead_].GetSamplesToFade());
    loopFade.Init(Fader::FadeType::FADE_SINGLE, samples, readRate_);
    activeReadHead_ = !activeReadHead_;
}

void Looper::UpdateReadPos()
{
    Head::Action activeAction = readHeads_[activeReadHead_].UpdatePosition();
    Head::Action inactiveAction = readHeads_[!activeReadHead_].UpdatePosition();
    Head::Action action = loopSync_ || loopLengthGrown_ ? activeAction : inactiveAction;

    // Note that in delay mode we don't need to fade the loop, and we couldn't do
    // it anyway because it'd need a few samples from outside the loop and these
    // samples are probably dirty.
    // Fading when changing the loop size yields the same problem, but it may be
    // necessary (which evil is the worse?).

    // When looping, the active reading head should fade out and proceed reading
    // ignoring the loop boundaries, while the inactive reading head should begin
    // a fade in from the other side of the loop.
    if (Head::Action::LOOP == action && !loopSync_ && (loopChanged_ || loopLength_ < bufferSamples_ || Direction::BACKWARDS == direction_))
    {
        SwitchReadingHeads();
        loopChanged_ = false;
    }
    else if (Head::Action::LOOP == action && loopSync_)
    {
        //readHeads_[0].ResetPosition();
        //readHeads_[1].ResetPosition();
    }
    else if (Head::Action::STOP == action)
    {
        Stop(true);
    }

    readPos_ = readHeads_[activeReadHead_].GetPosition();
    readPosSeconds_ = readPos_ / sampleRate_;
}

/*

writing head must reset position upon looping when in delay mode, but not if the loop has grown

*/

void Looper::UpdateWritePos()
{
    Head::Action action = writeHead_.UpdatePosition();
    writePos_ = writeHead_.GetIntPosition();
/*
    if (triggered && loopSync_)
    {
        if (loopLengthFade_)
        {
loopLengthFade_ = false;
        }
        else
        {

            writeHead_.ResetPosition();
        }
    }
*/
    // In delay mode, keep in sync the reading and the writing heads' position
    // each time the latter reaches either the start or the end of the loop
    // (depending on the reading direction).
    if (Head::Action::LOOP == action && loopSync_ && mustSyncHeads_ && readRate_ == 1.f)
    {
        readHeads_[0].SetIndex(writePos_);
        readHeads_[1].SetIndex(writePos_);
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