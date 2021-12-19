#pragma once

#include "looper.h"
//#include "envelope_follower.h"
#include "Utility/dsp.h"
#include "Filters/svf.h"
#include "dev/sdram.h"
#include <cmath>
#include <stddef.h>

namespace wreath
{
    using namespace daisysp;

    constexpr size_t kSampleRate{48000};
    constexpr int kBufferSeconds{150};                   // 2:30 minutes max
    const float kMinSamplesForTone{kSampleRate * 0.03f}; // 30ms
    const size_t kBufferSamples{kSampleRate * kBufferSeconds};

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

        void Init(size_t sampleRate)
        {
            sampleRate_ = sampleRate;
            loopers_[LEFT].Init(sampleRate_, leftBuffer_, kBufferSamples);
            loopers_[RIGHT].Init(sampleRate_, rightBuffer_, kBufferSamples);
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
                state_ = State::BUFFERING;
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
                static size_t fadeIndex{0};
                if (fadeIndex > sampleRate_)
                {
                    fadeIndex = 0;
                    state_ = State::BUFFERING;
                }
                fadeIndex++;
            }

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
                    loopers_[LEFT].Restart();
                    loopers_[RIGHT].Restart();
                }

                if (readingActive_)
                {
                    leftOut = loopers_[LEFT].Read(loopers_[LEFT].GetReadPos());
                    rightOut = loopers_[RIGHT].Read(loopers_[RIGHT].GetReadPos());
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

                size_t leftWritePos{loopers_[LEFT].GetWritePos()};
                size_t rightWritePos{loopers_[RIGHT].GetWritePos()};

                // Always write forward at original speed.
                leftWritePos += 1;
                rightWritePos += 1;
                loopers_[LEFT].SetWritePos(leftWritePos);
                loopers_[RIGHT].SetWritePos(rightWritePos);

                float leftReadPos{loopers_[LEFT].GetReadPos()};
                float rightReadPos{loopers_[RIGHT].GetReadPos()};

                float leftSpeedMult{loopers_[LEFT].GetSpeedMult()};
                float rightSpeedMult{loopers_[RIGHT].GetSpeedMult()};

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

                // Otherwise, move the reading position normally.
                hasChangedLeft_ = loopers_[LEFT].SetNextReadPos(loopers_[LEFT].IsGoingForward() ? loopers_[LEFT].GetReadPos() + leftSpeedMult : loopers_[LEFT].GetReadPos() - leftSpeedMult);
                hasChangedRight_ = loopers_[RIGHT].SetNextReadPos(loopers_[RIGHT].IsGoingForward() ? loopers_[RIGHT].GetReadPos() + rightSpeedMult : loopers_[RIGHT].GetReadPos() - rightSpeedMult);

                // Move smoothly to the next position.
                //fonepole(leftReadPos, loopers_[LEFT].GetNextReadPos(), leftCoeff);
                //fonepole(rightReadPos, loopers_[RIGHT].GetNextReadPos(), rightCoeff);
                leftReadPos = loopers_[LEFT].GetNextReadPos();
                rightReadPos = loopers_[RIGHT].GetNextReadPos();
                loopers_[LEFT].SetReadPos(leftReadPos);
                loopers_[RIGHT].SetReadPos(rightReadPos);
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
                loopers_[RIGHT].SetSpeedMult(loopers_[LEFT].GetSpeedMult());
                loopers_[RIGHT].SetLoopStart(loopers_[LEFT].GetLoopStart());
                loopers_[RIGHT].SetLoopLength(loopers_[LEFT].GetLoopLength());
                loopers_[RIGHT].SetReadPos(loopers_[LEFT].GetReadPos());
            }

            mode_ = mode;
        }

        inline size_t GetBufferSamples(int channel) { return loopers_[channel].GetBufferSamples(); }
        inline float GetBufferSeconds(int channel) { return loopers_[channel].GetBufferSeconds(); }
        inline float GetLoopStartSeconds(int channel) { return loopers_[channel].GetLoopStartSeconds(); }
        inline float GetLoopLengthSeconds(int channel) { return loopers_[channel].GetLoopLengthSeconds(); }
        inline float GetPositionSeconds(int channel) { return loopers_[channel].GetPositionSeconds(); }
        inline size_t GetLoopStart(int channel) { return loopers_[channel].GetLoopStart(); }
        inline size_t GetLoopEnd(int channel) { return loopers_[channel].GetLoopEnd(); }
        inline size_t GetLoopLength(int channel) { return loopers_[channel].GetLoopLength(); }
        inline float GetReadPos(int channel) { return loopers_[channel].GetReadPos(); }
        inline float GetWritePos(int channel) { return loopers_[channel].GetWritePos(); }
        inline float GetNextReadPos(int channel) { return loopers_[channel].GetNextReadPos(); }
        inline float GetSpeedMult(int channel) { return loopers_[channel].GetSpeedMult(); }
        inline Looper::Movement GetMovement(int channel) { return loopers_[channel].GetMovement(); }
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
        void SetMovement(int channel, Looper::Movement movement)
        {
            loopers_[channel].SetMovement(movement);
        }
        void SetLoopStart(int channel, size_t value)
        {
            loopers_[channel].SetLoopStart(value);
        }
        void SetSpeedMult(int channel, float multiplier)
        {
            loopers_[channel].SetSpeedMult(multiplier);
        }
        void SetLoopLength(int channel, size_t length)
        {
            loopers_[channel].SetLoopLength(length);
        }

        bool mustResetLooper{};
        bool mustClearBuffer{};
        bool mustStopBuffering{};
        bool mustRestart{};

        float nextGain{1.f};
        float nextMix{1.f};
        float nextFeedback{0.f};
        float nextFilterValue{0.f};

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
        size_t sampleRate_{};
        bool readingActive_{true};
        bool writingActive_{true};
        bool hasChangedLeft_{};
        bool hasChangedRight_{};
    };

}