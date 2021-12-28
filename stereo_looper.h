#pragma once

#include "head.h"
#include "looper.h"
#include "Utility/dsp.h"
#include "Filters/svf.h"
#include "dev/sdram.h"
#include <cmath>
#include <stddef.h>

namespace wreath
{
    using namespace daisysp;

    constexpr int32_t kSampleRate{48000};
    constexpr int kBufferSeconds{150};                   // 2:30 minutes max
    const float kMinSamplesForTone{kSampleRate * 0.03f}; // 30ms
    //const int32_t kBufferSamples{kSampleRate * kBufferSeconds};
    const int32_t kBufferSamples{48000};

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

        void Init(int32_t sampleRate)
        {
            sampleRate_ = sampleRate;
            loopers_[LEFT].Init(sampleRate_, leftBuffer_, kBufferSamples);
            loopers_[RIGHT].Init(sampleRate_, rightBuffer_, kBufferSamples);
            loopers_[LEFT].SetDirection(Direction::BACKWARDS);
            loopers_[RIGHT].SetDirection(Direction::BACKWARDS);
            state_ = State::INIT;
            cf_.Init(CROSSFADE_CPOW);
            feedbackFilter_.Init(sampleRate_);
            feedbackFilter_.SetFreq(filterValue_);
        }

        void ToggleFreeze()
        {
            state_ = IsFrozen() ? State::RECORDING : State::FROZEN;
            writingActive_ = !writingActive_;
            loopers_[LEFT].ToggleWriting();
            loopers_[RIGHT].ToggleWriting();
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
                loopers_[LEFT].Reset();
                loopers_[RIGHT].Reset();
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
                // Received the command to reset the read position to the loop start
                // point.
                if (mustRestart)
                {
                    mustRestart = false;
                    loopers_[LEFT].Restart();
                    loopers_[RIGHT].Restart();
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

                switch (mustSetChannelSpeedMult)
                {
                case BOTH:
                    loopers_[LEFT].SetRate(nextSpeedMult);
                    loopers_[RIGHT].SetRate(nextSpeedMult);
                    mustSetChannelSpeedMult = NONE;
                    break;
                case LEFT:
                case RIGHT:
                    loopers_[mustSetChannelSpeedMult].SetRate(nextSpeedMult);
                    mustSetChannelSpeedMult = NONE;
                    break;

                default:
                    break;
                }

                if (readingActive_)
                {
                    leftOut = loopers_[LEFT].Read();
                    rightOut = loopers_[RIGHT].Read();
                }

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

                if (writingActive_)
                {
                    float leftWet{};
                    float rightWet{};
                    if (readingActive_)
                    {
                        leftWet = leftOut * feedback_;
                        rightWet = rightOut * feedback_;
                        if (filterValue_ >= 20.f)
                        {
                            feedbackFilter_.Process(leftDry);
                            leftWet = SoftLimit(leftWet + feedbackFilter_.Band());
                            feedbackFilter_.Process(rightDry);
                            rightWet = SoftLimit(rightWet + feedbackFilter_.Band());
                        }
                    }
                    float dryLevel = 1.f - fmap(mix_ - 1.f, 0.f, 1.f);
                    loopers_[LEFT].Write(SoftLimit(leftDry * dryLevel + leftWet));
                    loopers_[RIGHT].Write(SoftLimit(rightDry * dryLevel + rightWet));
                }

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

                loopers_[LEFT].UpdateReadPos();
                loopers_[RIGHT].UpdateReadPos();
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
                loopers_[RIGHT].SetRate(loopers_[LEFT].GetRate());
                loopers_[RIGHT].SetLoopStart(loopers_[LEFT].GetLoopStart());
                loopers_[RIGHT].SetLoopLength(loopers_[LEFT].GetLoopLength());
                loopers_[RIGHT].UpdateReadPos();
            }

            mode_ = mode;
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
        inline float GetRate(int channel) { return loopers_[channel].GetRate(); }
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
        void SetRate(int channel, float multiplier)
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
        bool mustRestart{};

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

        inline float temp() { return loopers_[LEFT].temp; }

    private:
        Looper loopers_[2];
        float gain_{};
        float mix_{};
        float feedback_{};
        float filterValue_{};
        State state_{}; // The current state of the looper
        Mode mode_{};   // The current mode of the looper
        CrossFade cf_;
        Svf feedbackFilter_;
        //EnvFollow filterEnvelope_;
        int32_t sampleRate_{};
        bool readingActive_{true};
        bool writingActive_{true};
        bool hasChangedLeft_{};
        bool hasChangedRight_{};
    };

}