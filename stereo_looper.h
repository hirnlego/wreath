#pragma once

#include "head.h"
#include "looper.h"
#include "Utility/dsp.h"
#include "Filters/svf.h"
#include "Dynamics/crossfade.h"
#include "dev/sdram.h"
#include <cmath>
#include <stddef.h>

namespace wreath
{
    using namespace daisysp;

    constexpr int32_t kSampleRate{48000};
    //constexpr int kBufferSeconds{150}; // 2:30 minutes max
    constexpr int kBufferSeconds{1}; // 2:30 minutes max
    const int32_t kBufferSamples{kSampleRate * kBufferSeconds};

    float DSY_SDRAM_BSS leftBuffer_[kBufferSamples];
    float DSY_SDRAM_BSS rightBuffer_[kBufferSamples];

    class StereoLooper
    {
    public:
        StereoLooper() {}
        ~StereoLooper() {}

        enum
        {
            LEFT,
            RIGHT,
            BOTH,
            NONE,
        };

        enum State
        {
            INIT,
            BUFFERING,
            RECORDING,
            FROZEN,
        };

        enum Mode
        {
            MONO,
            CROSS,
            DUAL,
            LAST_MODE,
        };

        enum TriggerMode
        {
            TRIGGER,
            GATE,
            LOOP,
        };

        enum FilterType
        {
            LP,
            BP,
            HP,
        };

        struct Conf
        {
            Mode mode;
            TriggerMode triggerMode;
            Movement movement;
            Direction direction;
            float rate;
        };


        bool mustRestartRead{};
        bool mustResetLooper{};
        bool mustClearBuffer{};
        bool mustStopBuffering{};
        bool resetPosition{true};
        bool hasCvRestart{};

        float nextGain{1.f};
        float nextMix{1.f};
        float nextFeedback{0.f};
        float nextFilterValue{};

        bool noteModeLeft{};
        bool noteModeRight{};

        bool mustSetLeftLoopStart{};
        float nextLeftLoopStart{};
        float mustSetRightLoopStart{};
        int32_t nextRightLoopStart{};

        bool mustSetLeftDirection{};
        Direction nextLeftDirection{};
        bool mustSetRightDirection{};
        Direction nextRightDirection{};

        bool mustSetLeftLoopLength{};
        float nextLeftLoopLength{};
        bool mustSetRightLoopLength{};
        float nextRightLoopLength{};

        bool mustSetLeftReadRate{};
        float nextLeftReadRate{};
        bool mustSetRightReadRate{};
        float nextRightReadRate{};

        int mustSetChannelWriteRate{NONE};
        float nextWriteRate{};

        bool mustSetMode{};
        Mode nextMode{};

        bool mustSetTriggerMode{};
        TriggerMode nextTriggerMode{};

        bool mustStart{};
        bool mustStop{};
        bool mustRestart{};
        float stereoImage{1.f};
        float dryLevel{1.f};
        float filterResonance{0.45f};
        FilterType filterType{FilterType::BP};
        bool readingActive{true};

        void Reset()
        {
            loopers_[LEFT].Reset();
            loopers_[RIGHT].Reset();

            //SetMode(conf_.mode);
            //SetTriggerMode(conf_.triggerMode);
            //SetMovement(BOTH, conf_.movement);
            //SetDirection(BOTH, conf_.direction);
            //SetReadRate(BOTH, conf_.rate);
            //SetWriteRate(BOTH, conf_.rate);
        }

        void Init(int32_t sampleRate, Conf conf)
        {
            sampleRate_ = sampleRate;
            loopers_[LEFT].Init(sampleRate_, leftBuffer_, kBufferSamples);
            loopers_[RIGHT].Init(sampleRate_, rightBuffer_, kBufferSamples);
            state_ = State::INIT;
            cf_.Init(CROSSFADE_CPOW);
            feedbackFilter_.Init(sampleRate_);
            feedbackFilter_.SetFreq(filterValue_);
            outputFilter_.Init(sampleRate_);
            outputFilter_.SetFreq(filterValue_);

            // Process configuration and reset the looper.
            conf_ = conf;
            Reset();
        }

        void ToggleFreeze()
        {
            bool frozen = IsFrozen();
            SetFreeze(!frozen);
        }

        void SetFreeze(float value)
        {
            if (value < 0.5f && IsFrozen())
            {
                state_ = State::RECORDING;
            }
            else if (value >= 0.5f && !IsFrozen())
            {
                state_ = State::FROZEN;
            }
            loopers_[LEFT].SetWriting(value);
            loopers_[RIGHT].SetWriting(value);
            freeze_ = value;
        }

        float Filter(Svf *filter, float value)
        {
            filter->Process(value);
            switch (filterType)
            {
                case FilterType::BP:
                    return filter->Band();
                case FilterType::HP:
                    return filter->High();
                case FilterType::LP:
                    return filter->Low();
                default:
                    return filter->Band();
            }
        }

        void Process(const float leftIn, const float rightIn, float &leftOut, float &rightOut)
        {
            float leftWet{};
            float rightWet{};

            if (mustClearBuffer)
            {
                mustClearBuffer = false;
                loopers_[LEFT].ClearBuffer();
                loopers_[RIGHT].ClearBuffer();
            }

            if (mustResetLooper)
            {
                mustResetLooper = false;
                loopers_[LEFT].Stop(true);
                loopers_[RIGHT].Stop(true);
                Reset();
                mustSetTriggerMode = true;
                state_ = State::BUFFERING;
            }

            if (mustStopBuffering)
            {
                mustStopBuffering = false;
                loopers_[LEFT].StopBuffering();
                loopers_[RIGHT].StopBuffering();
                nextLeftLoopLength = loopers_[LEFT].GetLoopLength();
                nextRightLoopLength = loopers_[RIGHT].GetLoopLength();
                state_ = State::RECORDING;
            }

            // Wait a few samples to avoid potential clicking on module's startup.
            if (IsStartingUp())
            {
                static int32_t fadeIndex{0};
                if (fadeIndex > sampleRate_)
                {
                    fadeIndex = 0;
                    state_ = State::BUFFERING;
                }
                fadeIndex++;
            }

            // Update parameters.
            fonepole(feedback_, nextFeedback, 0.01f);
            fonepole(gain_, nextGain, 0.01f);
            fonepole(mix_, nextMix, 0.01f);
            fonepole(filterValue_, nextFilterValue, 0.01f);
            feedbackFilter_.SetFreq(filterValue_);
            feedbackFilter_.SetDrive(filterValue_ * 0.0001f);
            feedbackFilter_.SetRes(filterResonance + filterValue_ * 0.0005f);
            outputFilter_.SetFreq(filterValue_);
            outputFilter_.SetDrive(filterValue_ * 0.0001f);
            outputFilter_.SetRes(filterResonance + filterValue_ * 0.0005f);

            // Input gain stage.
            float leftDry{leftIn * gain_};
            float rightDry{rightIn * gain_};
            leftDry = (leftDry > 0) ? 1 - expf(-leftDry) : -1 + expf(leftDry);
            rightDry = (rightDry > 0) ? 1 - expf(-rightDry) : -1 + expf(rightDry);

            // Fill up the buffer for the first time.
            if (IsBuffering())
            {
                bool doneLeft{loopers_[LEFT].Buffer(leftDry)};
                bool doneRight{loopers_[RIGHT].Buffer(rightDry)};
                if (doneLeft && doneRight)
                {
                    mustStopBuffering = true;
                }
                leftWet = leftDry;
                rightWet = rightDry;
            }

            if (IsRecording() || IsFrozen())
            {
                /*
                if (mustRestartRead)
                {
                    mustRestartRead = false;
                }
                */

                if (mustSetMode)
                {
                    mustSetMode = false;
                    /*
                    loopers_[RIGHT].SetMovement(loopers_[LEFT].GetMovement());
                    loopers_[RIGHT].SetDirection(loopers_[LEFT].GetDirection());
                    loopers_[RIGHT].SetReadRate(loopers_[LEFT].GetReadRate());
                    loopers_[RIGHT].SetWriteRate(loopers_[LEFT].GetWriteRate());
                    loopers_[RIGHT].SetLoopLength(loopers_[LEFT].GetLoopLength());
                    loopers_[RIGHT].SetLoopStart(loopers_[LEFT].GetLoopStart());
                    loopers_[RIGHT].SetReadPos(loopers_[LEFT].GetReadPos());
                    */
                }

                if (mustSetTriggerMode)
                {
                    mustSetTriggerMode = false;
                    switch (nextTriggerMode)
                    {
                        case TriggerMode::GATE:
                            dryLevel = 0.f;
                            resetPosition = false;
                            loopers_[LEFT].SetReadPos(loopers_[LEFT].GetWritePos());
                            loopers_[RIGHT].SetReadPos(loopers_[RIGHT].GetWritePos());
                            loopers_[LEFT].SetLooping(true);
                            loopers_[RIGHT].SetLooping(true);
                            mustRestart = true;
                            break;
                        case TriggerMode::TRIGGER:
                            dryLevel = 1.f;
                            resetPosition = true;
                            loopers_[LEFT].SetLooping(false);
                            loopers_[RIGHT].SetLooping(false);
                            mustStop = true;
                            break;
                        case TriggerMode::LOOP:
                            dryLevel = 1.f;
                            resetPosition = true;
                            loopers_[LEFT].SetLooping(true);
                            loopers_[RIGHT].SetLooping(true);
                            mustStart = true;
                            break;
                    }
                }

                if (mustRestart)
                {
                    static bool dl{};
                    static bool dr{};
                    if (!dl)
                    {
                        dl = loopers_[LEFT].Restart(resetPosition);
                    }
                    if (!dr)
                    {
                        dr = loopers_[RIGHT].Restart(resetPosition);
                    }
                    if (dl && dr)
                    {
                        dl = dr = false;
                        mustRestart = false;
                    }
                }

                if (mustStart)
                {
                    bool doneLeft{loopers_[LEFT].Start()};
                    bool doneRight{loopers_[RIGHT].Start()};
                    if (doneLeft && doneRight)
                    {
                        readingActive = true;
                        mustStart = false;
                    }
                }

                if (mustStop)
                {
                    bool doneLeft{loopers_[LEFT].Stop(false)};
                    bool doneRight{loopers_[RIGHT].Stop(false)};
                    if (doneLeft && doneRight)
                    {
                        readingActive = false;
                        mustStop = false;
                    }
                }

                if (mustSetLeftDirection)
                {
                    loopers_[LEFT].SetDirection(nextLeftDirection);
                    mustSetLeftDirection = false;
                }
                if (mustSetRightDirection)
                {
                    loopers_[RIGHT].SetDirection(nextRightDirection);
                    mustSetRightDirection = false;
                }

                float leftLoopLength = loopers_[LEFT].GetLoopLength();
                if (nextLeftLoopLength != leftLoopLength)
                {
                    fonepole(leftLoopLength, nextLeftLoopLength, 0.0002f);
                    loopers_[LEFT].SetLoopLength(leftLoopLength);
                }

                float rightLoopLength = loopers_[RIGHT].GetLoopLength();
                if (nextRightLoopLength != rightLoopLength)
                {
                    fonepole(rightLoopLength, nextRightLoopLength, 0.0002f);
                    loopers_[RIGHT].SetLoopLength(rightLoopLength);
                }

                if (mustSetLeftLoopStart)
                {
                    loopers_[LEFT].SetLoopStart(nextLeftLoopStart);
                    mustSetLeftLoopStart = false;
                }
                if (mustSetRightLoopStart)
                {
                    loopers_[RIGHT].SetLoopStart(nextRightLoopStart);
                    mustSetRightLoopStart = false;
                }

                if (mustSetLeftReadRate)
                {
                    loopers_[LEFT].SetReadRate(nextLeftReadRate);
                    mustSetLeftReadRate = false;
                }
                if (mustSetRightReadRate)
                {
                    loopers_[RIGHT].SetReadRate(nextRightReadRate);
                    mustSetRightReadRate = false;
                }

                switch (mustSetChannelWriteRate)
                {
                case BOTH:
                    loopers_[LEFT].SetWriteRate(nextWriteRate);
                    loopers_[RIGHT].SetWriteRate(nextWriteRate);
                    mustSetChannelWriteRate = NONE;
                    break;
                case LEFT:
                case RIGHT:
                    loopers_[mustSetChannelWriteRate].SetWriteRate(nextWriteRate);
                    mustSetChannelWriteRate = NONE;
                    break;

                default:
                    break;
                }

                loopers_[LEFT].HandleFade();
                loopers_[RIGHT].HandleFade();

                leftWet = loopers_[LEFT].Read();
                rightWet = loopers_[RIGHT].Read();

                loopers_[LEFT].UpdateReadPos();
                loopers_[RIGHT].UpdateReadPos();

                float leftFeedback = leftWet * feedback_;
                float rightFeedback = rightWet * feedback_;
                if (filterValue_ >= 20.f)
                {
                    if (freeze_ > 0.f)
                    {
                        leftWet = Mix(leftWet, Filter(&outputFilter_, leftWet) * freeze_);
                        rightWet = Mix(rightWet, Filter(&outputFilter_, rightWet) * freeze_);
                    }
                    if (freeze_ < 1.f)
                    {
                        leftFeedback = Mix(leftFeedback, Filter(&feedbackFilter_, leftDry) * (1.f - freeze_));
                        rightFeedback = Mix(rightFeedback, Filter(&feedbackFilter_, rightDry) * (1.f - freeze_));
                    }
                }

                loopers_[LEFT].Write(Mix(leftDry * dryLevel, leftFeedback));
                loopers_[RIGHT].Write(Mix(rightDry * dryLevel, rightFeedback));

                loopers_[LEFT].UpdateWritePos();
                loopers_[RIGHT].UpdateWritePos();

                /*
                if (!hasCvRestart)
                {
                    // When drunk there's a small probability of changing direction.
                    bool toggleDir{rand() % loopers_[LEFT].GetSampleRateSpeed() == 1};
                    if (loopers_[LEFT].IsDrunkMovement())
                    {
                        if ((IsDualMode() && (rand() % loopers_[LEFT].GetSampleRateSpeed()) == 1) || toggleDir)
                        {
                            loopers_[LEFT].ToggleDirection();
                            hasChangedLeft_ = true;
                        }
                    }
                    if (loopers_[RIGHT].IsDrunkMovement())
                    {
                        if ((IsDualMode() && (rand() % loopers_[RIGHT].GetSampleRateSpeed()) == 1) || toggleDir)
                        {
                            loopers_[RIGHT].ToggleDirection();
                            hasChangedRight_ = true;
                        }
                    }
                }
                */
            }

            float stereoLeft = leftWet * stereoImage + rightWet * (1.f - stereoImage);
            float stereoRight = rightWet * stereoImage + leftWet * (1.f - stereoImage);

            cf_.SetPos(fclamp(mix_, 0.f, 1.f));
            leftOut = cf_.Process(leftDry, stereoLeft);
            rightOut = cf_.Process(rightDry, stereoRight);
        }

        void SetMode(Mode mode)
        {
            // When switching from dual mode to any mono mode, align the RIGHT
            // looper to the LEFT one.
            if (IsDualMode() && Mode::DUAL != mode)
            {
                mustSetMode = true;
                nextMode = mode;
            }

            conf_.mode = mode;
        }

        void SetTriggerMode(TriggerMode mode)
        {
            mustSetTriggerMode = true;
            nextTriggerMode = mode;
            conf_.triggerMode = mode;
        }

        void SetMovement(int channel, Movement movement)
        {
            if (BOTH == channel)
            {
                loopers_[LEFT].SetMovement(movement);
                loopers_[RIGHT].SetMovement(movement);
                conf_.movement = movement;
            }
            else{
                loopers_[channel].SetMovement(movement);
            }
        }
        void SetDirection(int channel, Direction direction)
        {
            if (LEFT == channel || BOTH == channel)
            {
                //conf_.direction = direction;
                nextLeftDirection = direction;
                mustSetLeftDirection = true;
            }
            if (RIGHT == channel || BOTH == channel)
            {
                //conf_.direction = direction;
                nextRightDirection = direction;
                mustSetRightDirection = true;
            }
        }
        void SetLoopStart(int channel, float value)
        {
            if (LEFT == channel || BOTH == channel)
            {
                nextLeftLoopStart = std::min(std::max(value, 0.f), loopers_[LEFT].GetBufferSamples() - 1.f);
                mustSetLeftLoopStart = true;
            }
            if (RIGHT == channel || BOTH == channel)
            {
                nextRightLoopStart = std::min(std::max(value, 0.f), loopers_[RIGHT].GetBufferSamples() - 1.f);
                mustSetRightLoopStart = true;
            }
        }
        void SetReadRate(int channel, float rate)
        {
            if (LEFT == channel || BOTH == channel)
            {
                //conf_.rate = rate;
                nextLeftReadRate = rate;
                mustSetLeftReadRate = true;
            }
            if (RIGHT == channel || BOTH == channel)
            {
                //conf_.rate = rate;
                mustSetRightReadRate = true;
                nextRightReadRate = rate;
            }
        }
        void SetWriteRate(int channel, float rate)
        {
            mustSetChannelWriteRate = channel;
            nextWriteRate = rate;
        }
        void SetLoopLength(int channel, float length)
        {
            if (LEFT == channel || BOTH == channel)
            {
                mustSetLeftLoopLength = true;
                nextLeftLoopLength = std::min(std::max(length, 0.f), static_cast<float>(loopers_[LEFT].GetBufferSamples()));
                //noteModeLeft = length == kMinSamplesForTone;
            }
            if (RIGHT == channel || BOTH == channel)
            {
                mustSetRightLoopLength = true;
                nextRightLoopLength = std::min(std::max(length, 0.f), static_cast<float>(loopers_[RIGHT].GetBufferSamples()));
                //noteModeRight = length == kMinSamplesForTone;
            }
        }

        inline int32_t GetBufferSamples(int channel) { return loopers_[channel].GetBufferSamples(); }
        inline float GetBufferSeconds(int channel) { return loopers_[channel].GetBufferSeconds(); }
        inline float GetLoopStartSeconds(int channel) { return loopers_[channel].GetLoopStartSeconds(); }
        inline float GetLoopLengthSeconds(int channel) { return loopers_[channel].GetLoopLengthSeconds(); }
        inline float GetReadPosSeconds(int channel) { return loopers_[channel].GetReadPosSeconds(); }
        inline float GetLoopStart(int channel) { return loopers_[channel].GetLoopStart(); }
        inline float GetLoopEnd(int channel) { return loopers_[channel].GetLoopEnd(); }
        inline float  GetLoopLength(int channel) { return loopers_[channel].GetLoopLength(); }
        inline float GetReadPos(int channel) { return loopers_[channel].GetReadPos(); }
        inline float GetWritePos(int channel) { return loopers_[channel].GetWritePos(); }
        inline float GetReadRate(int channel) { return loopers_[channel].GetReadRate(); }
        inline Movement GetMovement(int channel) { return loopers_[channel].GetMovement(); }
        inline bool IsGoingForward(int channel) { return loopers_[channel].IsGoingForward(); }
        inline int32_t GetCrossPoint(int channel) { return loopers_[channel].GetCrossPoint(); }
        inline int32_t GetHeadsDistance(int channel) { return loopers_[channel].GetHeadsDistance(); }

        inline bool IsStartingUp() { return State::INIT == state_; }
        inline bool IsBuffering() { return State::BUFFERING == state_; }
        inline bool IsRecording() { return State::RECORDING == state_; }
        inline bool IsFrozen() { return State::FROZEN == state_; }
        inline bool IsMonoMode() { return Mode::MONO == conf_.mode; }
        inline bool IsCrossMode() { return Mode::CROSS == conf_.mode; }
        inline bool IsDualMode() { return Mode::DUAL == conf_.mode; }
        inline Mode GetMode() { return conf_.mode; }
        inline TriggerMode GetTriggerMode() { return conf_.triggerMode; }
        inline float GetGain() { return gain_; }
        inline float GetMix() { return mix_; }
        inline float GetFeedBack() { return feedback_; }
        inline float GetFilter() { return filterValue_; }

    private:
        Looper loopers_[2];
        float gain_{};
        float mix_{};
        float feedback_{};
        float filterValue_{};
        State state_{}; // The current state of the looper
        CrossFade cf_;
        Svf feedbackFilter_;
        Svf outputFilter_;
        int32_t sampleRate_{};
        bool hasChangedLeft_{};
        bool hasChangedRight_{};
        Conf conf_{};
        float freeze_{};

        float Mix(float a, float b)
        {
            return SoftLimit(a + b);
            //return a + b - ((a * b) / 2.f);
            //return (1 / std::sqrt(2)) * (a + b);
        }
    };

}