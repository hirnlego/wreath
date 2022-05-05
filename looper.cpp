#include "looper.h"
#include "Utility/dsp.h"

using namespace wreath;
using namespace daisysp;

void Looper::Init(int32_t sampleRate, float *buffer, float *buffer2, int32_t maxBufferSamples)
{
    sampleRate_ = sampleRate;
    readHeads_[0].Init(buffer, buffer2, maxBufferSamples);
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
    readHeads_[0].SetActive(true);
    readHeads_[1].SetActive(true);
    readHeads_[0].SetLooping(true);
    readHeads_[1].SetLooping(true);
    writeRate_ = 1.f;
    writeHead_.SetRate(writeRate_);
    writeSpeed_ = sampleRate_ * writeRate_;
    writeHead_.SetActive(true);
    writeHead_.SetLooping(true);
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

void Looper::StartReading(bool now)
{
    if (readingActive_)
    {
        return;
    }

    readHeads_[0].SetActive(true);
    readHeads_[1].SetActive(true);
    readingActive_ = true;
    if (!now)
    {
        startReadingFade.Init(Fader::FadeType::FADE_SINGLE, kSamplesToFadeTrigger, readRate_);
    }
}

void Looper::StopReading(bool now)
{
    if (!readingActive_)
    {
        return;
    }

    if (now)
    {
        readHeads_[0].SetActive(false);
        readHeads_[1].SetActive(false);
        readingActive_ = false;
    }
    else
    {
        stopReadingFade.Init(Fader::FadeType::FADE_SINGLE, kSamplesToFadeTrigger, readRate_);
    }
}

void Looper::StartWriting(bool now)
{
    if (writingActive_)
    {
        return;
    }

    writingActive_ = true;
    if (!now)
    {
        startWritingFade.Init(Fader::FadeType::FADE_SINGLE, kSamplesToFadeTrigger, writeRate_);
    }
}

void Looper::StopWriting(bool now)
{
    if (!writingActive_)
    {
        return;
    }

    if (now)
    {
        writingActive_ = false;
    }
    else
    {
        stopWritingFade.Init(Fader::FadeType::FADE_SINGLE, kSamplesToFadeTrigger, writeRate_);
    }
}

void Looper::Trigger(bool restart)
{
    // Update the loop start
    readHeads_[0].SetLoopStart(loopStart_);
    readHeads_[1].SetLoopStart(loopStart_);

    // When a trigger is received while playing we fade out and then in the
    // reading, resetting the heads position in between.
    if (readingActive_)
    {
        StopReading(false);
        triggered_ = true;
    }
    // Otherwise, just read from the start.
    else if (restart)
    {
        readHeads_[0].ResetPosition();
        readHeads_[1].ResetPosition();
        if (loopSync_)
        {
            writeHead_.ResetPosition();
        }
        StartReading(false);
    }
}

void Looper::SetSamplesToFade(float samples)
{
    readHeads_[0].SetSamplesToFade(samples);
    readHeads_[1].SetSamplesToFade(samples);
    writeHead_.SetSamplesToFade(samples);
}

void Looper::SetLoopStart(float start)
{
    // Do not change value if there's a loop fade going.
    if (loopFade.IsActive() && loopLength_ > kMinSamplesForFlanger)
    {
        return;
    }

    loopLengthGrown_ = start > loopStart_;
    loopChanged_ = start != loopStart_;

    // Always change the inactive head first.
    loopStart_ = readHeads_[!activeReadHead_].SetLoopStart(start);

    // Also change the active one if the loop is short or we are not reading.
    if (loopLength_ <= kMinSamplesForFlanger || !readingActive_)
    {
        readHeads_[activeReadHead_].SetLoopStart(loopStart_);
        if (loopLength_ <= kMinSamplesForFlanger)
        {
            // Keep the heads inside the loop.
            readHeads_[0].ResetPosition();
            readHeads_[1].ResetPosition();
            if (loopSync_)
            {
                writeHead_.ResetPosition();
            }
        }
    }

    intLoopStart_ = loopStart_;
    loopStartSeconds_ = loopStart_ / static_cast<float>(sampleRate_);
    loopEnd_ = readHeads_[!activeReadHead_].GetLoopEnd();
    intLoopEnd_ = loopEnd_;
    crossPointFound_ = false;

    // In delay mode, keep the loop synched.
    if (loopSync_)
    {
        writeHead_.SetLoopStart(loopStart_);
    }
}

void Looper::SetLoopLength(float length)
{
    // Do not change value if there's a loop fade going.
    if (loopFade.IsActive() && loopLength_ > kMinSamplesForFlanger)
    {
        return;
    }

    loopLengthGrown_ = length > loopLength_;
    loopChanged_ = length != loopLength_;

    // Always change the inactive head first.
    loopLength_ = readHeads_[!activeReadHead_].SetLoopLength(length);

    // Also change the active one if the loop is short or we are not reading.
    if (length <= kMinSamplesForFlanger || !readingActive_)
    {
        readHeads_[activeReadHead_].SetLoopLength(loopLength_);
    }

    intLoopLength_ = loopLength_;
    loopLengthSeconds_ = loopLength_ / sampleRate_;
    loopEnd_ = readHeads_[!activeReadHead_].GetLoopEnd();
    intLoopEnd_ = loopEnd_;
    crossPointFound_ = false;

    // In delay mode, keep the loop synched.
    if (loopSync_)
    {
        writeHead_.SetLoopLength(loopLength_);
    }
}

void Looper::SetReadRate(float rate)
{
    readHeads_[0].SetRate(rate);
    readHeads_[1].SetRate(rate);
    readRate_ = rate;
    readSpeed_ = sampleRate_ * readRate_;
    sampleRateSpeed_ = static_cast<int32_t>(sampleRate_ / readRate_);
    crossPointFound_ = false;
    // In delay mode, when setting the rate back to 1 we flag for a realignment
    // of the heads at the next loop to keep the correct delay time.
    if (loopSync_ && rate == 1.f)
    {
        mustSyncHeads_ = true;
    }
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
    // If loopSync = true it means we're in delay mode, so the writing head must
    // loop when the reading head does.
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

float Looper::Read()
{
    float value = readHeads_[activeReadHead_].Read();

    // Fade in reading.
    if (startReadingFade.IsActive())
    {
        startReadingFade.Process(0, value);
        value = startReadingFade.GetOutput();
    }
    // Fade out reading.
    else if (stopReadingFade.IsActive())
    {
        if (Fader::FadeStatus::ENDED == stopReadingFade.Process(value, 0))
        {
            readHeads_[0].SetActive(false);
            readHeads_[1].SetActive(false);
            readingActive_ = false;
            // If the looper had been re-triggered while playing, at the end of
            // the fade out we reset the heads and then fade in reading.
            if (triggered_)
            {
                readHeads_[0].ResetPosition();
                readHeads_[1].ResetPosition();
                if (loopSync_)
                {
                    writeHead_.ResetPosition();
                }
                StartReading(false);
                triggered_ = false;
            }
        }
        value = stopReadingFade.GetOutput();
    }
    else if (!readingActive_)
    {
        return 0.f;
    }

    if (loopFade.IsActive())
    {
        if (Fader::FadeStatus::ENDED == loopFade.Process(readHeads_[!activeReadHead_].Read(), value))
        {
            readHeads_[!activeReadHead_].SetLoopStartAndLength(loopStart_, loopLength_);
            readHeads_[!activeReadHead_].SetIndex(readPos_);
            if (loopSync_)
            {
                writeHead_.SetIndex(readPos_);
            }
        }
        value = loopFade.GetOutput();
    }

    if (freeze_ > 0)
    {
        // Crossfade with the frozen buffer.
        value = Fader::EqualCrossFade(value, readHeads_[activeReadHead_].ReadFrozen(), freeze_);
    }

    // Handle fade on re-triggering.
    if (triggerFade.IsActive())
    {
        triggerFade.Process(0, value);
        value = triggerFade.GetOutput();
    }

    return value;
}

void Looper::Write(float input)
{
    // Fade in writing.
    if (startWritingFade.IsActive())
    {
        startWritingFade.Process(0, input);
        input = startWritingFade.GetOutput();
    }
    // Fade out writing.
    else if (stopWritingFade.IsActive())
    {
        if (Fader::FadeStatus::ENDED == stopWritingFade.Process(input, 0))
        {
            writingActive_ = false;
        }
        input = stopWritingFade.GetOutput();
    }
    else if (!writingActive_)
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

        // Use an Euclidean rhythm generator to apply degradation at fixed
        // buffer points
        return writeHead_.BresenhamEuclidean(eRand_ * 64, degradation_) ? input : input * d;
    }

    return input;
}

void Looper::FadeReadingToResetPosition()
{
    if (loopFade.IsActive())
    {
        return;
    }

    if (loopLengthGrown_)
    {
        readHeads_[activeReadHead_].ResetPosition();
        // When going backwards, if we don't have enough space for the fading of
        // the inactive reading head, we must reset its position.
        if (!IsGoingForward() && loopStart_ <= readHeads_[!activeReadHead_].GetSamplesToFade())
        {
            readHeads_[!activeReadHead_].ResetPosition();
        }
        loopLengthGrown_ = false;
    }
    else
    {
        readHeads_[!activeReadHead_].ResetPosition();
        // When going backwards, if we don't have enough space for the fading of
        // the active reading head, we must reset its position.
        if (!IsGoingForward() && loopStart_ <= readHeads_[activeReadHead_].GetSamplesToFade())
        {
            readHeads_[activeReadHead_].ResetPosition();
        }
    }

    // TODO: Probably the samples to fade could be calculated more precisely.
    // Active: length - buffer
    // inactive: length
    float samples = std::min(readHeads_[activeReadHead_].GetSamplesToFade(), readHeads_[!activeReadHead_].GetSamplesToFade());
    loopFade.Init(Fader::FadeType::FADE_SINGLE, samples, readRate_);
    activeReadHead_ = !activeReadHead_;
}

void Looper::UpdateReadPos()
{
    Head::Action action = readHeads_[activeReadHead_].UpdatePosition();

    // When the loop length shrunk, the inactive reading head dictates when
    // looping occurs, so we need to update its position as well.
    if (loopChanged_ && !loopLengthGrown_)
    {
        action = readHeads_[!activeReadHead_].UpdatePosition();
    }
    // Otherwise, just sync it with the active reading head.
    else
    {
        readHeads_[!activeReadHead_].SetIndex(readHeads_[activeReadHead_].GetPosition());
        readHeads_[!activeReadHead_].SetOffset(readHeads_[activeReadHead_].GetOffset());
    }

    // Note that in delay mode we don't need to fade the loop, and we wouldn't do
    // it anyway because it'd need a few samples from outside the loop and these
    // samples are probably unrelated.
    // Fading when the loop changes yields the same problem, but it sounds better
    // than if we don't.
    if (Head::Action::LOOP == action && loopLength_ > kMinSamplesForFlanger && (loopChanged_ || (!loopSync_ && loopLength_ < bufferSamples_)))
    {
        FadeReadingToResetPosition();
        loopChanged_ = false;
    }
    // Here we handle normal looping in delay mode or when the loop length is
    // small.
    else if (Head::Action::LOOP == action && (loopLength_ <= kMinSamplesForFlanger || loopSync_) && loopLength_ < bufferSamples_)
    {
        readHeads_[0].ResetPosition();
        readHeads_[1].ResetPosition();
    }
    else if (Head::Action::STOP == action)
    {
        StopReading(false);
    }

    readPos_ = readHeads_[activeReadHead_].GetPosition();
    readPosSeconds_ = readPos_ / sampleRate_;
}

void Looper::UpdateWritePos()
{
    Head::Action action = writeHead_.UpdatePosition();
    writePos_ = writeHead_.GetIntPosition();

    if (Head::Action::LOOP == action && loopSync_)
    {
        // Loop the writing head.
        if (loopLength_ < bufferSamples_)
        {
            writeHead_.ResetPosition();
        }

        // In delay mode, keep in sync the reading and the writing heads'
        // position each time the latter reaches either the start or the end of
        // the loop (depending on the reading direction).
        if (mustSyncHeads_)
        {
            readHeads_[0].ResetPosition();
            readHeads_[1].ResetPosition();
            mustSyncHeads_ = false;
        }
    }

    // When reading and writing speeds differ or we're going backwards, we
    // calculate the point where the two heads will meet and set up a writing
    // fade at that point.
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
            return (IsGoingForward() && aSpeed > bSpeed) ? loopLength_ - ((loopEnd_ - b) + (a - loopStart_)) : (loopEnd_ - b) + (a - loopStart_);
        }

        // Broken loop, case where b is in the second segment and a in the first.
        if (b >= loopStart_ && a <= loopEnd_)
        {
            return (!IsGoingForward() || bSpeed > aSpeed) ? loopLength_ - ((loopEnd_ - a) + (b - loopStart_)) : (loopEnd_ - a) + (b - loopStart_);
        }
    }

    if (a > b)
    {
        return (IsGoingForward() && aSpeed > bSpeed) ? loopLength_ - (a - b) : a - b;
    }

    return (!IsGoingForward() || bSpeed > aSpeed) ? loopLength_ - (b - a) : b - a;
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