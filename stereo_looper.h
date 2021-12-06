#pragma once

#include "looper.h"
#include "Utility/dsp.h"
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
            MODE2,
            DUAL,
            LAST_MODE,
        };

        void Init()
        {
            loopers_[LEFT].Init(kSampleRate, leftBuffer_, kBufferSeconds);
            loopers_[RIGHT].Init(kSampleRate, rightBuffer_, kBufferSeconds);

            state_ = State::INIT;

            cf_.Init(CROSSFADE_CPOW);
        }

        void ResetBuffer()
        {
            mustResetBuffer_ = true;
        }

        void StopBuffering()
        {
            mustStopBuffering_ = true;
        }

        void ToggleFreeze()
        {
            if (IsFrozen())
            {
                // Not frozen anymore.
                state_ = State::RECORDING;
                loopers_[LEFT].SetLoopStart(0);
                loopers_[RIGHT].SetLoopStart(0);
            }
            else
            {
                // Frozen.
                state_ = State::FROZEN;
                feedbackPickup_ = false;
            }
        }

        void Process(const float leftIn, const float rightIn, float &leftOut, float &rightOut)
        {
            leftOut = 0.f;
            rightOut = 0.f;

            if (mustResetBuffer_)
            {
                mustResetBuffer_ = false;
                feedback_ = 0.f;
                feedbackPickup_ = false;
                loopers_[LEFT].ResetBuffer();
                loopers_[RIGHT].ResetBuffer();
                state_ = State::BUFFERING;
            }

            if (mustStopBuffering_)
            {
                mustStopBuffering_ = false;
                loopers_[LEFT].StopBuffering();
                loopers_[RIGHT].StopBuffering();
                state_ = State::RECORDING;
            }

            // Wait a few samples to avoid potential clicking on module's startup.
            if (IsStartingUp())
            {
                static size_t fadeIndex{0};
                if (fadeIndex > kSampleRate)
                {
                    fadeIndex = 0;
                    state_ = State::BUFFERING;
                }
                fadeIndex++;
            }

            // Fill up the buffer the first time.
            if (IsBuffering())
            {
                bool doneLeft{loopers_[LEFT].Buffer(leftIn)};
                bool doneRight{loopers_[RIGHT].Buffer(rightIn)};
                if (doneLeft && doneRight)
                {
                    StopBuffering();
                }
            }

            if (IsRecording() || IsFrozen())
            {
                // Received the command to reset the read position to the loop start
                // point.
                if (mustRestart_)
                {
                    loopers_[LEFT].MustRestart();
                    loopers_[RIGHT].MustRestart();
                }

                leftOut = loopers_[LEFT].Read(loopers_[LEFT].GetReadPos());
                rightOut = loopers_[RIGHT].Read(loopers_[RIGHT].GetReadPos());

                float leftSpeed{loopers_[LEFT].GetSpeed()};
                float rightSpeed{loopers_[RIGHT].GetSpeed()};

                float leftWritePos{static_cast<float>(loopers_[LEFT].GetWritePos())};
                float rightWritePos{static_cast<float>(loopers_[RIGHT].GetWritePos())};

                if (IsMode2Mode())
                {
                    // In this mode the speed depends on the loop length.
                    leftSpeed = loopers_[LEFT].GetLoopLength() * (1.f / loopers_[LEFT].GetBufferSamples());
                    rightSpeed = loopers_[RIGHT].GetLoopLength() * (1.f / loopers_[RIGHT].GetBufferSamples());

                    float leftOut2{loopers_[LEFT].Read(loopers_[LEFT].GetWritePos())};
                    float rightOut2{loopers_[RIGHT].Read(loopers_[RIGHT].GetWritePos())};
                    // In this mode there always is writing, but when frozen writes the
                    // looped signal.
                    float leftWriteSig{IsFrozen() ? leftOut : leftIn + SoftLimit(leftOut2 * feedback_)};
                    float rightWriteSig{IsFrozen() ? rightOut : rightIn + SoftLimit(rightOut2 * feedback_)};
                    loopers_[LEFT].Write(leftWriteSig);
                    loopers_[RIGHT].Write(rightWriteSig);
                    leftWritePos = loopers_[LEFT].IsGoingForward() ? leftWritePos + leftSpeed : leftWritePos - leftSpeed;
                    rightWritePos = loopers_[RIGHT].IsGoingForward() ? rightWritePos + rightSpeed : rightWritePos - rightSpeed;
                    loopers_[LEFT].HandlePosBoundaries(leftWritePos, false);
                    loopers_[RIGHT].HandlePosBoundaries(rightWritePos, false);
                    loopers_[LEFT].SetWritePos(leftWritePos);
                    loopers_[RIGHT].SetWritePos(rightWritePos);
                }
                else
                {
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
                    else
                    {
                        loopers_[LEFT].Write(leftIn + SoftLimit(leftOut * feedback_));
                        loopers_[RIGHT].Write(rightIn + SoftLimit(rightOut * feedback_));
                    }

                    // Always write forward at original speed.
                    leftWritePos++;
                    rightWritePos++;
                    loopers_[LEFT].SetWritePos(leftWritePos);
                    loopers_[RIGHT].SetWritePos(rightWritePos);
                }

                float leftReadPos{loopers_[LEFT].GetReadPos()};
                float rightReadPos{loopers_[RIGHT].GetReadPos()};
                float leftCoeff{leftSpeed};
                float rightCoeff{rightSpeed};
                if (loopers_[LEFT].IsRandomMovement())
                {
                    // In this case we just choose randomly the next position.
                    if (std::abs(leftReadPos - loopers_[LEFT].GetNextReadPos()) < loopers_[LEFT].GetLoopLength() * 0.01f)
                    {
                        loopers_[LEFT].SetNextReadPos(loopers_[LEFT].GetRandomPosition());
                        loopers_[LEFT].SetForward(loopers_[LEFT].GetNextReadPos() > leftReadPos);
                    }
                    leftCoeff = 1.0f / ((2.f - leftSpeed) * kSampleRate);
                }
                else
                {
                    if (loopers_[LEFT].IsDrunkMovement())
                    {
                        // When drunk there's a small probability of changing direction.
                        if ((rand() % kSampleRate) == 1)
                        {
                            loopers_[LEFT].ToggleDirection();
                        }
                    }
                    // Otherwise, move the reading position normally.
                    loopers_[LEFT].SetNextReadPos(loopers_[LEFT].IsGoingForward() ? loopers_[LEFT].GetReadPos() + leftSpeed : loopers_[LEFT].GetReadPos() - leftSpeed);
                }

                if (loopers_[RIGHT].IsRandomMovement())
                {
                    // In this case we just choose randomly the next position.
                    if (std::abs(rightReadPos - loopers_[RIGHT].GetNextReadPos()) < loopers_[RIGHT].GetLoopLength() * 0.01f)
                    {
                        loopers_[RIGHT].SetNextReadPos(loopers_[RIGHT].GetRandomPosition());
                        loopers_[RIGHT].SetForward(loopers_[RIGHT].GetNextReadPos() > rightReadPos);
                    }
                    rightCoeff = 1.0f / ((2.f - rightSpeed) * kSampleRate);
                }
                else
                {
                    if (loopers_[RIGHT].IsDrunkMovement())
                    {
                        // When drunk there's a small probability of changing direction.
                        if ((rand() % kSampleRate) == 1)
                        {
                            loopers_[RIGHT].ToggleDirection();
                        }
                    }
                    // Otherwise, move the reading position normally.
                    loopers_[RIGHT].SetNextReadPos(loopers_[RIGHT].IsGoingForward() ? loopers_[RIGHT].GetReadPos() + rightSpeed : loopers_[RIGHT].GetReadPos() - rightSpeed);
                }

                // Move smoothly to the next position.
                fonepole(leftReadPos, loopers_[LEFT].GetNextReadPos(), leftCoeff);
                fonepole(rightReadPos, loopers_[RIGHT].GetNextReadPos(), rightCoeff);
                loopers_[LEFT].HandlePosBoundaries(leftReadPos, true);
                loopers_[RIGHT].HandlePosBoundaries(rightReadPos, true);
                loopers_[LEFT].SetReadPos(leftReadPos);
                loopers_[RIGHT].SetReadPos(rightReadPos);

                float left{leftIn};
                float right{rightIn};
                cf_.SetPos(dryWet_);
                leftOut = cf_.Process(left, leftOut);
                rightOut = cf_.Process(right, rightOut);
            }
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
        inline float GetSpeed(int channel) { return loopers_[channel].GetSpeed(); }
        inline Looper::Movement GetMovement(int channel) { return loopers_[channel].GetMovement(); }
        inline bool IsGoingForward(int channel) { return loopers_[channel].IsGoingForward(); }

        inline bool IsStartingUp() { return State::INIT == state_; }
        inline bool IsBuffering() { return State::BUFFERING == state_; }
        inline bool IsRecording() { return State::RECORDING == state_; }
        inline bool IsFrozen() { return State::FROZEN == state_; }
        inline bool IsMimeoMode() { return Mode::MIMEO == mode_; }
        inline bool IsMode2Mode() { return Mode::MODE2 == mode_; }
        inline bool IsDualMode() { return Mode::DUAL == mode_; }
        inline Mode GetMode() { return mode_; }
        void SetMode(Mode mode)
        {
            // When switching from dual mode to a coupled mode, reset the
            // loopers.
            if (IsDualMode() && Mode::DUAL != mode)
            {
                loopers_[LEFT].SetMovement(Looper::Movement::FORWARD);
                loopers_[RIGHT].SetMovement(Looper::Movement::FORWARD);
                loopers_[LEFT].SetSpeed(1.0f);
                loopers_[RIGHT].SetSpeed(1.0f);
                loopers_[LEFT].ResetLoopLength();
                loopers_[RIGHT].ResetLoopLength();
                loopers_[LEFT].Restart();
                loopers_[RIGHT].Restart();
            }

            mode_ = mode;
        }
        inline void SetDryWet(float dryWet) { dryWet_ = dryWet; }
        inline void SetFeedback(float feedback) { feedback_ = feedback; }
        inline void SetMovement(int channel, Looper::Movement movement) { loopers_[channel].SetMovement(movement); }
        inline void IncrementSpeed(int channel, float value) { loopers_[channel].SetSpeed(loopers_[channel].GetSpeed() + value); }
        inline void IncrementLoopLength(int channel, size_t samples)
        {
            if (samples > 0)
            {
                loopers_[channel].IncrementLoopLength(samples);
            }
            else if (samples < 0)
            {
                loopers_[channel].DecrementLoopLength(samples);
            }
        }
        inline void Restart() {  mustRestart_ = true; }

    private:
        Looper loopers_[2];
        float dryWet_{};
        float feedback_{};
        State state_{}; // The current state of the looper
        Mode mode_{}; // The current mode of the looper
        CrossFade cf_;
        bool feedbackPickup_{};
        bool mustRestart_{};
        bool mustResetBuffer_{};
        bool mustStopBuffering_{};
    };


}