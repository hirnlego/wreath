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
    constexpr int kBufferSeconds{150};                   // 2:30 minutes max
    const float kMinSamplesForTone{kSampleRate * 0.03f}; // 30ms
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
            NONE,
            BOTH,
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

        struct Conf
        {
            Mode mode;
            TriggerMode triggerMode;
            Movement movement;
            Direction direction;
            float rate;
        };

        void Reset()
        {
            loopers_[LEFT].Reset();
            loopers_[RIGHT].Reset();

            SetMode(conf_.mode);
            SetTriggerMode(conf_.triggerMode);
            SetMovement(BOTH, conf_.movement);
            SetDirection(BOTH, conf_.direction);
            SetReadRate(BOTH, conf_.rate);
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
        }

        void Process(const float leftIn, const float rightIn, float &leftOut, float &rightOut)
        {
            leftOut = 0.f;
            rightOut = 0.f;

            if (mustClearBuffer)
            {
                mustClearBuffer = false;
                loopers_[LEFT].ClearBuffer();
                loopers_[RIGHT].ClearBuffer();
            }

            if (mustResetLooper)
            {
                mustResetLooper = false;
                Reset();
                state_ = State::BUFFERING;
            }

            if (mustStopBuffering)
            {
                mustStopBuffering = false;
                loopers_[LEFT].StopBuffering();
                loopers_[RIGHT].StopBuffering();
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
            feedbackFilter_.SetRes(0.45f + filterValue_ * 0.0005f);

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
            }

            if (IsRecording() || IsFrozen())
            {
                if (mustRestart)
                {
                    bool doneLeft{loopers_[LEFT].Restart()};
                    bool doneRight{loopers_[RIGHT].Restart()};
                    if (doneLeft && doneRight)
                    {
                        mustRestart = false;
                    }
                }

                if (mustStart)
                {
                    bool doneLeft{loopers_[LEFT].Start()};
                    bool doneRight{loopers_[RIGHT].Start()};
                    if (doneLeft && doneRight)
                    {
                        mustStart = false;
                    }
                }

                if (mustStop)
                {
                    bool doneLeft{loopers_[LEFT].Stop()};
                    bool doneRight{loopers_[RIGHT].Stop()};
                    if (doneLeft && doneRight)
                    {
                        mustStop = false;
                    }
                }

                switch (mustSetChannelSpeedMult)
                {
                case BOTH:
                    loopers_[LEFT].SetReadRate(nextSpeedMult);
                    loopers_[RIGHT].SetReadRate(nextSpeedMult);
                    mustSetChannelSpeedMult = NONE;
                    break;
                case LEFT:
                case RIGHT:
                    loopers_[mustSetChannelSpeedMult].SetReadRate(nextSpeedMult);
                    mustSetChannelSpeedMult = NONE;
                    break;

                default:
                    break;
                }

                switch (mustSetChannelLoopLength)
                {
                case BOTH:
                    loopers_[LEFT].SetLoopLength(nextLoopLength);
                    loopers_[RIGHT].SetLoopLength(nextLoopLength);
                    mustSetChannelLoopLength = NONE;
                    break;
                case LEFT:
                case RIGHT:
                    loopers_[mustSetChannelLoopLength].SetLoopLength(nextLoopLength);
                    mustSetChannelLoopLength = NONE;
                    break;

                default:
                    break;
                }

                switch (mustSetChannelLoopStart)
                {
                case BOTH:
                    loopers_[LEFT].SetLoopStart(nextLoopStart);
                    loopers_[RIGHT].SetLoopStart(nextLoopStart);
                    mustSetChannelLoopStart = NONE;
                    break;
                case LEFT:
                case RIGHT:
                    loopers_[mustSetChannelLoopStart].SetLoopStart(nextLoopStart);
                    mustSetChannelLoopStart = NONE;
                    break;

                default:
                    break;
                }

                loopers_[LEFT].HandleFade();
                loopers_[RIGHT].HandleFade();

                leftOut = loopers_[LEFT].Read();
                rightOut = loopers_[RIGHT].Read();

                loopers_[LEFT].UpdateReadPos();
                loopers_[RIGHT].UpdateReadPos();

                /*
                // In cross mode swap the two channels, so what's read in the
                // left buffer is written in the right one and vice-versa.
                if (IsCrossMode())
                {
                    float temp = leftOut;
                    if (hasChangedLeft_)
                    {
                        leftOut = rightOut;
                    }
                    if (hasChangedRight_)
                    {
                        rightOut = temp;
                    }
                }
                */

                float leftWet = leftOut * feedback_;
                float rightWet = rightOut * feedback_;
                if (filterValue_ >= 20.f)
                {
                    feedbackFilter_.Process(leftDry);
                    leftWet = Mix(leftWet, feedbackFilter_.Band());
                    feedbackFilter_.Process(rightDry);
                    rightWet = Mix(rightWet, feedbackFilter_.Band());
                }
                float dryLevel = 1.f - fmap(mix_ - 1.f, 0.f, 1.f);
                loopers_[LEFT].Write(Mix(leftDry * dryLevel, leftWet));
                loopers_[RIGHT].Write(Mix(rightDry * dryLevel, rightWet));

                loopers_[LEFT].UpdateWritePos();
                loopers_[RIGHT].UpdateWritePos();

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
            }

            cf_.SetPos(fclamp(mix_, 0.f, 1.f));
            leftOut = cf_.Process(leftDry, leftOut);
            rightOut = cf_.Process(rightDry, rightOut);
        }

        void SetMode(Mode mode)
        {
            // When switching from dual mode to any mono mode, align the RIGHT
            // looper to the LEFT one.
            if (IsDualMode() && Mode::DUAL != mode)
            {
                loopers_[RIGHT].SetMovement(loopers_[LEFT].GetMovement());
                loopers_[RIGHT].SetReadRate(loopers_[LEFT].GetReadRate());
                loopers_[RIGHT].SetLoopStart(loopers_[LEFT].GetLoopStart());
                loopers_[RIGHT].SetLoopLength(loopers_[LEFT].GetLoopLength());
                loopers_[RIGHT].UpdateReadPos();
            }

            mode_ = mode;
        }

        void SetTriggerMode(TriggerMode mode)
        {
            switch (mode)
            {
                case TriggerMode::GATE:
                    loopers_[LEFT].Stop();
                    loopers_[RIGHT].Stop();
                    loopers_[LEFT].SetLooping(true);
                    loopers_[RIGHT].SetLooping(true);
                    break;
                case TriggerMode::TRIGGER:
                    loopers_[LEFT].Stop();
                    loopers_[RIGHT].Stop();
                    loopers_[LEFT].SetLooping(false);
                    loopers_[RIGHT].SetLooping(false);
                    break;
                case TriggerMode::LOOP:
                    loopers_[LEFT].Start();
                    loopers_[RIGHT].Start();
                    loopers_[LEFT].SetLooping(true);
                    loopers_[RIGHT].SetLooping(true);
                    break;
            }
            triggerMode_ = mode;
        }

        inline int32_t GetBufferSamples(int channel) { return loopers_[channel].GetBufferSamples(); }
        inline float GetBufferSeconds(int channel) { return loopers_[channel].GetBufferSeconds(); }
        inline float GetLoopStartSeconds(int channel) { return loopers_[channel].GetLoopStartSeconds(); }
        inline float GetLoopLengthSeconds(int channel) { return loopers_[channel].GetLoopLengthSeconds(); }
        inline float GetReadPosSeconds(int channel) { return loopers_[channel].GetReadPosSeconds(); }
        inline int32_t GetLoopStart(int channel) { return loopers_[channel].GetLoopStart(); }
        inline int32_t GetLoopEnd(int channel) { return loopers_[channel].GetLoopEnd(); }
        inline int32_t GetLoopLength(int channel) { return loopers_[channel].GetLoopLength(); }
        inline float GetReadPos(int channel) { return loopers_[channel].GetReadPos(); }
        inline float GetWritePos(int channel) { return loopers_[channel].GetWritePos(); }
        inline float GetNextReadPos(int channel) { return loopers_[channel].GetNextReadPos(); }
        inline float GetReadRate(int channel) { return loopers_[channel].GetReadRate(); }
        inline Movement GetMovement(int channel) { return loopers_[channel].GetMovement(); }
        inline bool IsGoingForward(int channel) { return loopers_[channel].IsGoingForward(); }

        inline bool IsStartingUp() { return State::INIT == state_; }
        inline bool IsBuffering() { return State::BUFFERING == state_; }
        inline bool IsRecording() { return State::RECORDING == state_; }
        inline bool IsFrozen() { return State::FROZEN == state_; }
        inline bool IsMonoMode() { return Mode::MONO == mode_; }
        inline bool IsCrossMode() { return Mode::CROSS == mode_; }
        inline bool IsDualMode() { return Mode::DUAL == mode_; }
        inline Mode GetMode() { return mode_; }
        inline TriggerMode GetTriggerMode() { return triggerMode_; }
        inline float GetGain() { return gain_; }
        inline float GetMix() { return mix_; }
        inline float GetFeedBack() { return feedback_; }
        inline float GetFilter() { return filterValue_; }

        void SetReading(bool active)
        {
            readingActive_ = active;
            loopers_[LEFT].SetReading(active);
            loopers_[RIGHT].SetReading(active);
        }
        void SetMovement(int channel, Movement movement)
        {
            if (BOTH == channel)
            {
                loopers_[LEFT].SetMovement(movement);
                loopers_[RIGHT].SetMovement(movement);
            }
            else{
                loopers_[channel].SetMovement(movement);
            }
        }
        void SetDirection(int channel, Direction direction)
        {
            if (BOTH == channel)
            {
                loopers_[LEFT].SetDirection(direction);
                loopers_[RIGHT].SetDirection(direction);
            }
            else{
                loopers_[channel].SetDirection(direction);
            }
        }
        void SetLoopStart(int channel, int32_t value)
        {
            mustSetChannelLoopStart = channel;
            nextLoopStart = value;
        }
        void SetReadRate(int channel, float multiplier)
        {
            mustSetChannelSpeedMult = channel;
            nextSpeedMult = multiplier;
        }
        void SetLoopLength(int channel, int32_t length)
        {
            mustSetChannelLoopLength = channel;
            nextLoopLength = length;
        }

        bool mustResetLooper{};
        bool mustClearBuffer{};
        bool mustStopBuffering{};
        bool hasCvRestart{};

        float nextGain{1.f};
        float nextMix{1.f};
        float nextFeedback{0.f};
        float nextFilterValue{};

        int mustSetChannelLoopStart{NONE};
        int32_t nextLoopStart{};

        int mustSetChannelLoopLength{NONE};
        int32_t nextLoopLength{};

        int mustSetChannelSpeedMult{NONE};
        float nextSpeedMult{};

        bool mustStart{};
        bool mustStop{};
        bool mustRestart{};

    private:
        Looper loopers_[2];
        float gain_{};
        float mix_{};
        float feedback_{};
        float filterValue_{};
        State state_{}; // The current state of the looper
        Mode mode_{};   // The current mode of the looper
        TriggerMode triggerMode_{};
        CrossFade cf_;
        Svf feedbackFilter_;
        //EnvFollow filterEnvelope_;
        int32_t sampleRate_{};
        bool readingActive_{true};
        bool hasChangedLeft_{};
        bool hasChangedRight_{};
        Conf conf_{};

        float Mix(float a, float b)
        {
            return SoftLimit(a + b);
            //return a + b - ((a * b) / 2.f);
            //return (1 / std::sqrt(2)) * (a + b);
        }
    };

}