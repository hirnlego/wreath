/**
 * Inspired by Monome softcut's subhead class:
 * https://github.com/monome/softcut-lib/blob/main/softcut-lib/src/SubHead.cpp
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace wreath
{
    constexpr int kMinLoopLengthSamples{48};
    constexpr int kMinLoopLengthForFade{4800};
    constexpr float kMinSamplesForTone{1440}; // 30ms @ 48KHz
    constexpr int32_t kSamplesToFade{1200};
    constexpr float kSwitchAndRampThresh{0.2f};

    enum Type
    {
        READ,
        WRITE,
    };

    enum Action
    {
        NO_ACTION,
        LOOP,
        INVERT,
        STOP,
    };

    enum Movement
    {
        NORMAL,
        PENDULUM,
        DRUNK,
    };

    enum Direction
    {
        BACKWARDS = -1,
        FORWARD = 1
    };

    enum RunStatus
    {
        STOPPED,
        RUNNING,
        STOPPING,
        STARTING,
        RESTARTING,
    };

    class Head
    {
    private:
        const Type type_;
        float *buffer_;

        int32_t maxBufferSamples_{}; // The whole buffer length in samples
        int32_t bufferSamples_{};    // The written buffer length in samples

        int32_t intIndex_{};
        float index_{};
        float rate_{};
        float fadeIndex_{};

        int32_t loopStart_{};
        int32_t loopEnd_{};
        int32_t loopLength_{};

        bool looping_{};
        float snapshotValue_{};
        bool switchAndRamp_{};
        RunStatus runStatus_{};

        Movement movement_{};
        Direction direction_{};

        float previousValue_{};
        float currentValue_{};
        float writeBalance_{}; // Balance between new and old value when writing

        int32_t samplesToFade_{kSamplesToFade};
        float fadeRate_{1.f};

        Action HandleLoopAction()
        {
            // Handle normal loop boundaries.
            if (loopEnd_ > loopStart_)
            {
                // Forward direction.
                if (Direction::FORWARD == direction_)
                {
                    // This prevents "dragging" the index while changing the
                    // loop's start point.
                    if (intIndex_ < loopStart_)
                    {
                        SetIndex(loopStart_);

                        return Action::LOOP;
                    }
                    if (looping_ && intIndex_ > loopEnd_)
                    {
                        if (Movement::PENDULUM == movement_ && looping_)
                        {
                            SetIndex(loopEnd_ - (index_ - loopEnd_));

                            return Action::INVERT;
                        }
                        else
                        {
                            SetIndex((loopStart_ + (index_ - loopEnd_)) - 1);

                            return Action::LOOP;
                        }
                    }
                    // When the head is not looping, and while it's not already
                    // stopping, stop it and allow for a fade out.
                    else if (RunStatus::STOPPING != runStatus_ && !looping_ && intIndex_ > loopEnd_ - SamplesToFade())
                    {
                        return Action::STOP;
                    }
                }
                // Backwards direction.
                else {
                    // This prevents "dragging" the index while changing the
                    // loop's start point.
                    if (intIndex_ > loopEnd_)
                    {
                        SetIndex(loopEnd_);

                        return Action::LOOP;
                    }
                    if (looping_ && intIndex_ < loopStart_)
                    {
                        if (Movement::PENDULUM == movement_ && looping_)
                        {
                            SetIndex(loopStart_ + (loopStart_ - index_));

                            return Action::INVERT;
                        }
                        else
                        {
                            SetIndex((loopEnd_ - std::abs(loopStart_ - index_)) + 1);

                            return Action::LOOP;
                        }
                    }
                    // When the head is not looping, and while it's not already
                    // stopping, stop it and allow for a fade out.
                    else if (!looping_ && intIndex_ < loopStart_ + SamplesToFade())
                    {
                        if (RunStatus::STOPPING == runStatus_)
                        {
                            SetIndex((loopEnd_ - std::abs(loopStart_ - index_)) + 1);

                            return Action::LOOP;
                        }

                        return Action::STOP;
                    }
                }
            }
            // Handle inverted loop boundaries (end point comes before start point).
            else
            {
                float frame = bufferSamples_ - 1;
                if (Direction::FORWARD == direction_)
                {
                    if (intIndex_ > frame)
                    {
                        // Wrap-around.
                        SetIndex((index_ - frame) - 1);

                        return looping_ ? Action::LOOP : Action::STOP;
                    }
                    else if (intIndex_ > loopEnd_ && intIndex_ < loopStart_)
                    {
                        if (Movement::PENDULUM == movement_ && looping_)
                        {
                            // Max to avoid overflow.
                            SetIndex(std::max(loopEnd_ - (index_ - loopEnd_), 0.f));

                            return Action::INVERT;
                        }
                        else
                        {
                            // Min to avoid overflow.
                            SetIndex(std::min(loopStart_ + (index_ - loopEnd_) - 1, frame));

                            return looping_ ? Action::LOOP : Action::STOP;
                        }
                    }
                }
                else
                {
                    if (intIndex_ < 0)
                    {
                        // Wrap-around.
                        SetIndex((frame - std::abs(index_)) + 1);

                        return looping_ ? Action::LOOP : Action::STOP;
                    }
                    else if (intIndex_ > loopEnd_ && intIndex_ < loopStart_)
                    {
                        if (Movement::PENDULUM == movement_ && looping_)
                        {
                            // Min to avoid overflow.
                            SetIndex(std::min(loopStart_ + (loopStart_ - index_), frame));

                            return Action::INVERT;
                        }
                        else
                        {
                            // Max to avoid overflow.
                            SetIndex(std::max(loopEnd_ - (loopStart_ - index_) + 1, 0.f));

                            return looping_ ? Action::LOOP : Action::STOP;
                        }
                    }
                }
            }

            return Action::NO_ACTION;
        }

        int32_t WrapIndex(int32_t index)
        {
            // Handle normal loop boundaries.
            if (loopEnd_ > loopStart_)
            {
                // Forward direction.
                if (index > loopEnd_)
                {
                    if (Movement::PENDULUM == movement_)
                    {
                        index = loopEnd_ - (index - loopEnd_);
                    }
                    else
                    {
                        index = (FORWARD == direction_) ? (loopStart_ + (index - loopEnd_)) - 1 : 0;
                    }
                }
                // Backwards direction.
                else if (index < loopStart_)
                {
                    if (Movement::PENDULUM == movement_)
                    {
                        index = loopStart_ + (loopStart_ - index);
                    }
                    else
                    {
                        index = (BACKWARDS == direction_) ? (loopEnd_ - std::abs(loopStart_ - index)) + 1 : 0;
                    }
                }
            }
            // Handle inverted loop boundaries (end point comes before start point).
            else
            {
                int32_t frame{bufferSamples_ - 1};
                if (index > frame)
                {
                    index = (index - frame) - 1;
                }
                else if (index < 0)
                {
                    // Wrap-around.
                    index = (frame - std::abs(index)) + 1;
                }
                else if (index > loopEnd_ && index < loopStart_)
                {
                    if (FORWARD == direction_)
                    {
                        // Max/min to avoid overflow.
                        index = (Movement::PENDULUM == movement_) ? std::max(loopEnd_ - (index - loopEnd_), static_cast<int32_t>(0)) : std::min(loopStart_ + (index - loopEnd_) - 1, frame);
                    }
                    else
                    {
                        // Max/min to avoid overflow.
                        index = (Movement::PENDULUM == movement_) ? std::min(loopStart_ + (loopStart_ - index), frame) : std::max(loopEnd_ - (loopStart_ - index) + 1, static_cast<int32_t>(0));
                    }
                }
            }

            return index;
        }

        void CalculateLoopEnd()
        {
            if (loopStart_ + loopLength_ > bufferSamples_)
            {
                loopEnd_ = (loopStart_ + loopLength_) - bufferSamples_ - 1;
            }
            else
            {
                loopEnd_ = loopStart_ + loopLength_ - 1;
            }
        }

        float CrossFade(float from, float to, float pos)
        {
            float in = std::sin(1.570796326794897 * pos);
            float out = std::cos(1.570796326794897 * pos);

            return from * out + to * in;
        }

    public:
        Head(Type type) : type_{type} {}
        ~Head() {}

        void Reset()
        {
            intIndex_ = 0;
            index_ = 0.f;
            loopStart_ = 0;
            loopEnd_ = 0;
        }

        void Init(float *buffer, int32_t maxBufferSamples)
        {
            buffer_ = buffer;
            maxBufferSamples_ = maxBufferSamples;
            rate_ = 1.f;
            looping_ = false;
            runStatus_ = RunStatus::STOPPED;
            movement_ = Movement::NORMAL;
            direction_ = Direction::FORWARD;
            Reset();
        }

        int32_t SetLoopStart(int32_t start)
        {
            loopStart_ = std::min(std::max(start, static_cast<int32_t>(0)), bufferSamples_ - 1);
            CalculateLoopEnd();
            if (!looping_)
            {
                ResetPosition();
            }
            switchAndRamp_ = false;

            return loopStart_;
        }

        int32_t SetLoopLength(int32_t length)
        {
            loopLength_ = std::min(std::max(length, static_cast<int32_t>(kMinLoopLengthSamples)), bufferSamples_);
            CalculateLoopEnd();
            switchAndRamp_ = false;

            return loopLength_;
        }

        int32_t SamplesToFade()
        {
            return std::min(kSamplesToFade, loopLength_);
        }

        inline void SetWriteBalance(float amount) { writeBalance_ = amount; }
        inline void SetRate(float rate)
        {
            rate_ = std::abs(rate);
            switchAndRamp_ = false;
        }
        inline void SetMovement(Movement movement)
        {
            movement_ = movement;
            switchAndRamp_ = false;
        }
        inline void SetDirection(Direction direction)
        {
            direction_ = direction;
            switchAndRamp_ = false;
        }
        inline void SetIndex(float index)
        {
            index_ = index;
            intIndex_ = static_cast<int32_t>(std::floor(index));
        }
        inline void ResetPosition()
        {
            SetIndex(FORWARD == direction_ ? loopStart_ : loopEnd_);
        }
        float UpdatePosition()
        {
            if (RunStatus::STOPPED == runStatus_)
            {
                //return index_;
            }

            float index = index_ + (rate_ * direction_);
            SetIndex(index);
            Action action = HandleLoopAction();

            if (READ == type_)
            {
                switch (action)
                {
                case STOP:
                    Stop();
                    break;

                case INVERT:
                    ToggleDirection();
                    break;

                case LOOP:
                    break;

                default:
                    break;
                }
            }

            return index_;
        }

        bool IsFading()
        {
            return switchAndRamp_;
        }

        void SetFade(int32_t samples, float rate)
        {
            samplesToFade_ = samples;
            fadeRate_ = rate;
        }

        void SwitchAndRamp()
        {
            snapshotValue_ = previousValue_;
            switchAndRamp_ = true;
            fadeIndex_ = 0;
            samplesToFade_ = std::min(static_cast<int32_t>(std::abs(snapshotValue_ - currentValue_) * 1000), loopLength_);
        }

        float Read()
        {
            if (RunStatus::STOPPED == runStatus_)
            {
                return 0.f;
            }

            previousValue_ = currentValue_;
            float value = ReadAt(index_);
            currentValue_ = value;

            // Gradually start reading, fading from zero to the buffered value.
            if (RunStatus::STARTING == runStatus_)
            {
                value = CrossFade(0, value, fadeIndex_ * (1.f / samplesToFade_));
                fadeIndex_ += rate_;
                if (fadeIndex_ > samplesToFade_)
                {
                    runStatus_ = RunStatus::RUNNING;
                }
            }
            // Gradually stop reading, fading from the buffered value to zero.
            else if (RunStatus::STOPPING == runStatus_)
            {
                value = CrossFade(value, 0, fadeIndex_ * (1.f / samplesToFade_));
                fadeIndex_ += rate_;
                if (fadeIndex_ > samplesToFade_)
                {
                    runStatus_ = RunStatus::STOPPED;
                }
            }
            else
            {
                // Apply switch-and-ramp technique to smooth the read value if
                // the difference from the previous value is more than the
                // defined threshold.
                // http://msp.ucsd.edu/techniques/v0.11/book-html/node63.html

                /* FIX: this generates artefacts with high frequency signals
                if (!switchAndRamp_ && std::abs(previousValue_ - value) > kSwitchAndRampThresh && loopLength_ > kMinSamplesForTone)
                {
                    SwitchAndRamp();
                }
                */

                if (switchAndRamp_)
                {
                    float delta = snapshotValue_ - value;
                    float win = std::sin(1.570796326794897 * fadeIndex_ * (1.f / samplesToFade_));
                    //win = fadeIndex_ * (1.f / samplesToFade_);
                    value += delta * (1.0f - win);
                    fadeIndex_ += rate_;
                    if (fadeIndex_ > samplesToFade_)
                    {
                        switchAndRamp_ = false;
                    }
                }
            }

            return value;
        }

        float ReadAt(float index)
        {
            int32_t intPos = static_cast<int32_t>(std::floor(index));
            float value = buffer_[intPos];
            float frac = index - intPos;

            // Interpolate value only it the index has a fractional part.
            if (frac > std::numeric_limits<float>::epsilon())
            {
                float value2 = buffer_[WrapIndex(intPos + direction_)];

                return value + (value2 - value) * frac;
            }

            return value;
        }

        void Write(float value)
        {
            currentValue_ = ReadAt(index_);
            //value = value * (1.f - writeBalance_) + currentValue_ * writeBalance_;

            // Gradually start writing, fading from the buffered value to the input
            // signal.
            if (RunStatus::STARTING == runStatus_)
            {
                value = CrossFade(currentValue_, value, fadeIndex_ * (1.f / samplesToFade_));
                fadeIndex_ += fadeRate_;
                if (fadeIndex_ > samplesToFade_)
                {
                    runStatus_ = RunStatus::RUNNING;
                }
            }
            // Gradually stop writing, fading from the input signal to the buffered
            // value.
            else if (RunStatus::STOPPING == runStatus_)
            {
                value = CrossFade(value, currentValue_, fadeIndex_ * (1.f / samplesToFade_));
                fadeIndex_ += fadeRate_;
                if (fadeIndex_ > samplesToFade_)
                {
                    runStatus_ = RunStatus::STOPPED;
                }
            }

            if (RunStatus::STOPPED != runStatus_)
            {
                buffer_[WrapIndex(intIndex_)] = value;
            }
        }

        void ClearBuffer()
        {
            memset(buffer_, 0.f, maxBufferSamples_);
        }

        bool Buffer(float value)
        {
            if (intIndex_ < maxBufferSamples_ - kSamplesToFade)
            {
                buffer_[intIndex_] = value;
            }
            // Fade out recording until reaching the end of the available buffer.
            else
            {
                // Handle end of buffer.
                if (intIndex_ > maxBufferSamples_ - 1)
                {
                    return true;
                }

                int32_t d = std::abs(maxBufferSamples_ - intIndex_ - kSamplesToFade);
                buffer_[intIndex_] = value * (1.f - d / static_cast<float>(kSamplesToFade));
            }

            intIndex_++;
            bufferSamples_ = intIndex_;

            return false;
        }

        void InitBuffer(int32_t bufferSamples)
        {
            bufferSamples_ = bufferSamples;
            loopLength_ = bufferSamples_;
            loopEnd_ = loopLength_ - 1;
        }

        int32_t StopBuffering()
        {
            intIndex_ = 0;
            loopLength_ = bufferSamples_;
            loopEnd_ = loopLength_ - 1;
            ResetPosition();

            return bufferSamples_;
        }

        inline Direction ToggleDirection()
        {
            direction_ = static_cast<Direction>(direction_ * -1);

            return direction_;
        }

        inline void SetRunStatus(RunStatus status)
        {
            runStatus_ = status;
            fadeIndex_ = 0;
        }

        RunStatus Start()
        {
            // Start right away if the loop length is smaller than the minimum
            // number of samples needed for a tone.
            RunStatus status = loopLength_ > kMinSamplesForTone ? RunStatus::STARTING : RunStatus::RUNNING;
            SetRunStatus(status);

            return status;
        }

        RunStatus Stop()
        {
            // Stop right away if the loop length is smaller than the minimum
            // number of samples needed for a tone.
            RunStatus status = loopLength_ > kMinSamplesForTone ? RunStatus::STOPPING : RunStatus::STOPPED;
            SetRunStatus(status);

            return status;
        }

        inline void SetLooping(bool looping)
        {
            looping_ = looping;
        }

        inline int32_t GetBufferSamples() { return bufferSamples_; }
        inline int32_t GetLoopEnd() { return loopEnd_; }
        inline float GetRate() { return rate_; }
        inline float GetPosition() { return index_; }
        inline int32_t GetIntPosition() { return intIndex_; }
        bool IsGoingForward() { return Direction::FORWARD == direction_; }
        bool IsRunning() { return RunStatus::RUNNING == runStatus_; }
        RunStatus GetRunStatus() { return runStatus_; }
    };
}