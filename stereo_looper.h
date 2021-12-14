#pragma once

#include "looper.h"
#include "Utility/dsp.h"
#include "Filters/tone.h"
#include "dev/sdram.h"
#include <cmath>
#include <stddef.h>

namespace wreath
{
    using namespace daisysp;

    constexpr size_t kSampleRate{48000};
    constexpr int kBufferSeconds{150}; // 2:30 minutes max
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
            MIMEO,
            CROSS,
            //MODE2,
            DUAL,
            LAST_MODE,
        };

        void Init(size_t sampleRate)
        {
            sampleRate_ = sampleRate;
            loopers_[LEFT].Init(sampleRate_, leftBuffer_, kBufferSeconds);
            loopers_[RIGHT].Init(sampleRate_, rightBuffer_, kBufferSeconds);
            state_ = State::INIT;
            cf_.Init(CROSSFADE_CPOW);
            lpf_.Init(sampleRate_);
            float f{1000.f};
            lpf_.SetFreq(f);
        }

        void ToggleFreeze()
        {
            if (IsFrozen())
            {
                // Un-freeze.
                state_ = State::RECORDING;
                //loopers_[LEFT].SetLoopStart(0);
                //loopers_[RIGHT].SetLoopStart(0);
            }
            else
            {
                // Freeze.
                state_ = State::FROZEN;
                feedbackPickup_ = false;
            }
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
                gain_ = 1.f;
                feedback_ = 0.f;
                feedbackPickup_ = false;
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

            // Gain stage.
            float left{leftIn * gain_};
            float right{rightIn * gain_};
            left = (left > 0) ? 1 - expf(-left) : -1 + expf(left);
            right = (right > 0) ? 1 - expf(-right) : -1 + expf(right);

            // Fill up the buffer for the first time.
            if (IsBuffering())
            {
                bool doneLeft{loopers_[LEFT].Buffer(left)};
                bool doneRight{loopers_[RIGHT].Buffer(right)};
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

                leftOut = loopers_[LEFT].Read(loopers_[LEFT].GetReadPos());
                rightOut = loopers_[RIGHT].Read(loopers_[RIGHT].GetReadPos());

                // In cross mode swap the two channels, so what's read in the
                // left buffer is written in the right one and vice-versa.
                if (IsCrossMode())
                {
                    float temp = leftOut;
                    leftOut = rightOut;
                    rightOut = temp;
                }

                if (!IsFrozen())
                {
                    float leftWet{leftOut * feedback_};
                    float rightWet{rightOut * feedback_};
                    // Apply a LPF on the feedback path only if the loop is sufficiently long.
                    if (loopers_[LEFT].GetLoopLength() > 4800)
                    {
                        lpf_.Process(leftWet);
                    }
                    if (loopers_[RIGHT].GetLoopLength() > 4800)
                    {
                        lpf_.Process(rightWet);
                    }
                    loopers_[LEFT].Write(SoftLimit(left + leftWet));
                    loopers_[RIGHT].Write(SoftLimit(right + rightWet));
                }

                cf_.SetPos(dryWet_);
                leftOut = cf_.Process(left, leftOut);
                rightOut = cf_.Process(right, rightOut);

                /* TODO
                if (IsFrozen())
                {
                    // When frozen, the feedback value sets the starting point.
                    size_t leftStart{static_cast<size_t>(std::floor(feedback_ * loopers_[LEFT].GetBufferSamples()))};
                    size_t rightStart{static_cast<size_t>(std::floor(feedback_ * loopers_[RIGHT].GetBufferSamples()))};

                    // Pick up where the loop start point is.
                    // TODO: handle both LEFT and RIGHT loopers.
                    if (std::abs(static_cast<int>(leftStart - loopers_[LEFT].GetLoopStart())) < static_cast<int>(loopers_[LEFT].GetBufferSamples() * 0.1f) && !feedbackPickup_)
                    {
                        feedbackPickup_ = true;
                    }
                    if (feedbackPickup_)
                    {
                        loopers_[LEFT].SetLoopStart(leftStart);
                        loopers_[RIGHT].SetLoopStart(rightStart);
                    }
                    if (loopers_[LEFT].GetLoopStart() + loopers_[LEFT].GetLoopLength() > loopers_[LEFT].GetBufferSamples())
                    {
                        loopers_[LEFT].SetLoopEnd(loopers_[LEFT].GetLoopStart() + loopers_[LEFT].GetLoopLength() - loopers_[LEFT].GetBufferSamples());
                        loopers_[RIGHT].SetLoopEnd(loopers_[RIGHT].GetLoopStart() + loopers_[RIGHT].GetLoopLength() - loopers_[RIGHT].GetBufferSamples());
                    }
                    else
                    {
                        loopers_[LEFT].SetLoopEnd(loopers_[LEFT].GetLoopStart() + loopers_[LEFT].GetLoopLength() - 1);
                        loopers_[RIGHT].SetLoopEnd(loopers_[RIGHT].GetLoopStart() + loopers_[RIGHT].GetLoopLength() - 1);
                    }
                    // Note that in this mode no writing is done while frozen.
                }
                */

                float leftWritePos{static_cast<float>(loopers_[LEFT].GetWritePos())};
                float rightWritePos{static_cast<float>(loopers_[RIGHT].GetWritePos())};

                // Always write forward at original speed.
                leftWritePos += 1;
                rightWritePos += 1;
                loopers_[LEFT].SetWritePos(leftWritePos);
                loopers_[RIGHT].SetWritePos(rightWritePos);

                float leftReadPos{loopers_[LEFT].GetReadPos()};
                float rightReadPos{loopers_[RIGHT].GetReadPos()};

                float leftSpeedMult{loopers_[LEFT].GetSpeedMult()};
                float rightSpeedMult{loopers_[RIGHT].GetSpeedMult()};

                bool toggleDir{rand() % loopers_[LEFT].GetSampleRateSpeed()  == 1};
                //bool toggleDir{rand() % sampleRate_  == 1};

                float leftCoeff{leftSpeedMult};
                float rightCoeff{rightSpeedMult};
                if (loopers_[LEFT].IsRandomMovement())
                {
                    /*
                    // In this case we just choose randomly the next position.
                    if (std::abs(leftReadPos - loopers_[LEFT].GetNextReadPos()) < loopers_[LEFT].GetLoopLength() * 0.01f)
                    {
                        loopers_[LEFT].SetNextReadPos(loopers_[LEFT].GetRandomPosition());
                        loopers_[LEFT].SetForward(loopers_[LEFT].GetNextReadPos() > leftReadPos);
                    }
                    leftCoeff = 1.0f / ((2.f - leftSpeedMult) * sampleRate_);
                    */
                }
                else
                {
                    if (loopers_[LEFT].IsDrunkMovement())
                    {
                        // When drunk there's a small probability of changing direction.
                        if ((IsDualMode() && (rand() % loopers_[LEFT].GetSampleRateSpeed()) == 1) || toggleDir)
                        {
                            loopers_[LEFT].ToggleDirection();
                        }
                    }
                    // Otherwise, move the reading position normally.
                    loopers_[LEFT].SetNextReadPos(loopers_[LEFT].IsGoingForward() ? loopers_[LEFT].GetReadPos() + leftSpeedMult : loopers_[LEFT].GetReadPos() - leftSpeedMult);
                }

                if (loopers_[RIGHT].IsRandomMovement())
                {
                    /*
                    // In this case we just choose randomly the next position.
                    if (std::abs(rightReadPos - loopers_[RIGHT].GetNextReadPos()) < loopers_[RIGHT].GetLoopLength() * 0.01f)
                    {
                        loopers_[RIGHT].SetNextReadPos(loopers_[RIGHT].GetRandomPosition());
                        loopers_[RIGHT].SetForward(loopers_[RIGHT].GetNextReadPos() > rightReadPos);
                    }
                    rightCoeff = 1.0f / ((2.f - rightSpeedMult) * sampleRate_);
                    */
                }
                else
                {
                    if (loopers_[RIGHT].IsDrunkMovement())
                    {
                        // When drunk there's a small probability of changing direction.
                        if ((IsDualMode() && (rand() % loopers_[RIGHT].GetSampleRateSpeed()) == 1) || toggleDir)
                        {
                            loopers_[RIGHT].ToggleDirection();
                        }
                    }
                    // Otherwise, move the reading position normally.
                    loopers_[RIGHT].SetNextReadPos(loopers_[RIGHT].IsGoingForward() ? loopers_[RIGHT].GetReadPos() + rightSpeedMult : loopers_[RIGHT].GetReadPos() - rightSpeedMult);
                }

                // Move smoothly to the next position.
                //fonepole(leftReadPos, loopers_[LEFT].GetNextReadPos(), leftCoeff);
                //fonepole(rightReadPos, loopers_[RIGHT].GetNextReadPos(), rightCoeff);
                leftReadPos = loopers_[LEFT].GetNextReadPos();
                rightReadPos = loopers_[RIGHT].GetNextReadPos();
                loopers_[LEFT].SetReadPos(leftReadPos);
                loopers_[RIGHT].SetReadPos(rightReadPos);
            }
        }

        void SetMode(Mode mode)
        {
            // When switching from dual mode to a coupled mode, reset the
            // loopers.
            if (IsDualMode() && Mode::DUAL != mode)
            {
                loopers_[LEFT].SetMovement(Looper::Movement::FORWARD);
                loopers_[RIGHT].SetMovement(Looper::Movement::FORWARD);
                loopers_[LEFT].SetSpeedMult(1.0f);
                loopers_[RIGHT].SetSpeedMult(1.0f);
                loopers_[LEFT].ResetLoopLength();
                loopers_[RIGHT].ResetLoopLength();
                loopers_[LEFT].Restart();
                loopers_[RIGHT].Restart();
            }

            mode_ = mode;
        }

        inline size_t GetBufferSamples(int channel) { return loopers_[channel].GetBufferSamples(); }
        inline float GetBufferSeconds(int channel) { return loopers_[channel].GetBufferSeconds(); }
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
        inline bool IsMimeoMode() { return Mode::MIMEO == mode_; }
        inline bool IsCrossMode() { return Mode::CROSS == mode_; }
        //inline bool IsMode2Mode() { return Mode::MODE2 == mode_; }
        inline bool IsDualMode() { return Mode::DUAL == mode_; }
        inline Mode GetMode() { return mode_; }
        inline float GetGain() { return gain_; }
        inline void SetGain(float gain) { gain_ = gain; }
        inline void SetDryWet(float dryWet) { dryWet_ = dryWet; }
        inline void SetFeedback(float feedback) { feedback_ = feedback; }
        inline void SetMovement(int channel, Looper::Movement movement) { loopers_[channel].SetMovement(movement); }
        inline void SetLoopStart(size_t value)
        {
            loopers_[LEFT].SetNextReadPos(value);
            loopers_[RIGHT].SetNextReadPos(value);
        }
        inline void IncrementSpeedMult(int channel, float value) { loopers_[channel].SetSpeedMult(loopers_[channel].GetSpeedMult() + value); }
        inline void IncrementLoopLength(int channel, size_t samples) { loopers_[channel].SetLoopLength(loopers_[channel].GetLoopLength() + samples); }

        bool mustResetLooper{};
        bool mustClearBuffer{};
        bool mustStopBuffering{};
        bool mustRestart{};

        inline float temp() { return loopers_[LEFT].temp; }

    private:
        Looper loopers_[2];
        float gain_{1.f};
        float dryWet_{};
        float feedback_{};
        State state_{}; // The current state of the looper
        Mode mode_{}; // The current mode of the looper
        CrossFade cf_;
        Tone lpf_;
        size_t sampleRate_{};
        bool feedbackPickup_{};
    };


}