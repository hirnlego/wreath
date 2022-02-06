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
    constexpr float kMinLoopLengthSamples{48.f};
    constexpr int kMinLoopLengthForFade{4800};
    constexpr float kMinSamplesForTone{1440}; // 30ms @ 48KHz
    constexpr float kSamplesToFade{1200.f};
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

        float loopStart_{};
        int32_t intLoopStart_{};
        float loopEnd_{};
        int32_t intLoopEnd_{};
        float loopLength_{};
        int32_t intLoopLength_{};

        bool looping_{};
        float snapshotValue_{};
        float switchAndRampPos_{};
        bool mustSwitchAndRamp_{};
        bool switchAndRamp_{};
        RunStatus runStatus_{};

        Movement movement_{};
        Direction direction_{};

        float previousValue_{};
        float currentValue_{};
        float writeBalance_{}; // Balance between new and old value when writing

        float samplesToFade_{kSamplesToFade};
        float fadeRate_{1.f};

        Action HandleLoopAction()
        {
            // Handle normal loop boundaries.
            if (intLoopEnd_ > intLoopStart_)
            {
                // Forward direction.
                if (Direction::FORWARD == direction_)
                {
                    // This prevents "dragging" the index while changing the
                    // loop's start point.
                    if (intIndex_ < intLoopStart_)
                    {
                        SetIndex(loopStart_);

                        return Action::LOOP;
                    }
                    if (looping_ && intIndex_ > intLoopEnd_)
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
                    else if (RunStatus::RUNNING == runStatus_ && !looping_ && intIndex_ > intLoopEnd_ - SamplesToFade())
                    {
                        return Action::STOP;
                    }
                }
                // Backwards direction.
                else {
                    // This prevents "dragging" the index while changing the
                    // loop's start point.
                    if (intIndex_ > intLoopEnd_)
                    {
                        SetIndex(loopEnd_);

                        return Action::LOOP;
                    }
                    if (looping_ && intIndex_ < intLoopStart_)
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
                    else if (!looping_ && intIndex_ < intLoopStart_ + SamplesToFade())
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
                    else if (intIndex_ > intLoopEnd_ && intIndex_ < intLoopStart_)
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
                    else if (intIndex_ > intLoopEnd_ && intIndex_ < intLoopStart_)
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
            if (intLoopEnd_ > intLoopStart_)
            {
                // Forward direction.
                if (index > intLoopEnd_)
                {
                    if (Movement::PENDULUM == movement_)
                    {
                        index = intLoopEnd_ - (index - intLoopEnd_);
                    }
                    else
                    {
                        index = (FORWARD == direction_) ? (intLoopStart_ + (index - intLoopEnd_)) - 1 : 0;
                    }
                }
                // Backwards direction.
                else if (index < intLoopStart_)
                {
                    if (Movement::PENDULUM == movement_)
                    {
                        index = intLoopStart_ + (intLoopStart_ - index);
                    }
                    else
                    {
                        index = (BACKWARDS == direction_) ? (intLoopEnd_ - std::abs(intLoopStart_ - index)) + 1 : 0;
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
                else if (index > intLoopEnd_ && index < intLoopStart_)
                {
                    if (FORWARD == direction_)
                    {
                        // Max/min to avoid overflow.
                        index = (Movement::PENDULUM == movement_) ? std::max(intLoopEnd_ - (index - intLoopEnd_), static_cast<int32_t>(0)) : std::min(intLoopStart_ + (index - intLoopEnd_) - 1, frame);
                    }
                    else
                    {
                        // Max/min to avoid overflow.
                        index = (Movement::PENDULUM == movement_) ? std::min(intLoopStart_ + (intLoopStart_ - index), frame) : std::max(intLoopEnd_ - (intLoopStart_ - index) + 1, static_cast<int32_t>(0));
                    }
                }
            }

            return index;
        }

        void CalculateLoopEnd()
        {
            if (intLoopStart_ + intLoopLength_ > bufferSamples_)
            {
                loopEnd_ = (loopStart_ + loopLength_) - bufferSamples_ - 1;
            }
            else
            {
                loopEnd_ = loopStart_ + loopLength_ - 1;
            }
            intLoopEnd_ = loopEnd_;
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
            intLoopStart_ = 0;
            intLoopEnd_ = 0;
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

        float SetLoopStart(float start)
        {
            loopStart_ = start;
            intLoopStart_ = loopStart_;
            CalculateLoopEnd();
            if (!looping_)
            {
                ResetPosition();
            }
            switchAndRamp_ = false;

            return loopStart_;
        }

        float SetLoopLength(float length)
        {
            loopLength_ = length;
            intLoopLength_ = loopLength_;
            CalculateLoopEnd();
            switchAndRamp_ = false;

            return loopLength_;
        }

        float SamplesToFade()
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
            intIndex_ = index_;
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

        void SetFade(float samples, float rate)
        {
            samplesToFade_ = samples;
            fadeRate_ = rate;
        }

        void SetSwitchAndRamp(float start)
        {
            switchAndRamp_ = true;
            switchAndRampPos_ = start;
            fadeIndex_ = 0;
        }

        void SwitchAndRamp()
        {
            snapshotValue_ = previousValue_;
            switchAndRamp_ = true;
            fadeIndex_ = 0;
            //samplesToFade_ = std::min(static_cast<int32_t>(std::abs(snapshotValue_ - currentValue_) * 1000), loopLength_);
            samplesToFade_ = SamplesToFade();
        }

        float Read(float valueToFade)
        {
            if (RunStatus::STOPPED == runStatus_)
            {
                return 0.f;
            }

            previousValue_ = currentValue_;
            float value = ReadAt(index_);
            currentValue_ = value;

            // Gradually start reading, fading from the input signal to the
            // buffered value.
            if (RunStatus::STARTING == runStatus_)
            {
                valueToFade = switchAndRamp_ ? ReadAt(switchAndRampPos_ + fadeIndex_) : valueToFade;
                value = CrossFade(valueToFade, value, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    if (switchAndRamp_)
                    {
                        switchAndRamp_ = false;
                    }
                    runStatus_ = RunStatus::RUNNING;
                }
                fadeIndex_ += rate_;
            }
            // Gradually stop reading, fading from the buffered value to the
            // input signal.
            else if (RunStatus::STOPPING == runStatus_)
            {
                value = CrossFade(value, valueToFade, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::STOPPED;
                }
                fadeIndex_ += rate_;
            }
            else
            {
                // Apply switch-and-ramp technique to smooth the read value if
                // the difference from the previous value is more than the
                // defined threshold.
                // http://msp.ucsd.edu/techniques/v0.11/book-html/node63.html

                // FIX: this generates artefacts with high frequency signals
                /*
                if (!switchAndRamp_ && std::abs(previousValue_ - value) > kSwitchAndRampThresh && loopLength_ > kMinSamplesForTone)
                {
                    //SwitchAndRamp();
                }
                */
/*
                if (switchAndRamp_)
                {
                    float delta = snapshotValue_ - value;
                    float win = std::sin(1.570796326794897 * fadeIndex_ * (1.f / samplesToFade_));
                    //win = fadeIndex_ * (1.f / samplesToFade_);
                    value += delta * (1.0f - win);
                    if (fadeIndex_ >= samplesToFade_)
                    {
                        switchAndRamp_ = false;
                    }
                    fadeIndex_ += rate_;
                }
                */
            }

            return value;
        }

        float ReadAt(float index)
        {
            int32_t intPos = index;
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

        void Write(float input)
        {
            currentValue_ = ReadAt(index_);
            //value = value * (1.f - writeBalance_) + currentValue_ * writeBalance_;

            if (loopLength_ < bufferSamples_ && index_ == loopEnd_ - samplesToFade_)
            {
                float i = samplesToFade_ - (loopEnd_ - index_);
                input = CrossFade(input, currentValue_, i * (1.f / samplesToFade_));
            }

            // Gradually start writing, fading from the buffered value to the input
            // signal.
            if (RunStatus::STARTING == runStatus_)
            {
                input = CrossFade(currentValue_, input, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::RUNNING;
                }
                fadeIndex_ += fadeRate_;
            }
            // Gradually stop writing, fading from the input signal to the buffered
            // value.
            else if (RunStatus::STOPPING == runStatus_)
            {
                input = CrossFade(input, currentValue_, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::STOPPED;
                }
                fadeIndex_ += fadeRate_;
            }

            if (RunStatus::STOPPED != runStatus_)
            {
                buffer_[WrapIndex(intIndex_)] = input;
            }
        }

        void ClearBuffer()
        {
            memset(buffer_, 0.f, maxBufferSamples_);
        }

        bool Buffer(float value)
        {
            buffer_[intIndex_] = value;

            // End of available buffer?
            if (intIndex_ == maxBufferSamples_ - 1)
            {
                return true;
            }

            intIndex_++;
            bufferSamples_ = intIndex_;

            return false;
        }

        void InitBuffer(int32_t bufferSamples)
        {
            bufferSamples_ = bufferSamples;
            loopLength_ = bufferSamples_;
            intLoopLength_ = loopLength_;
            loopEnd_ = loopLength_ - 1.f;
            intLoopEnd_ = loopEnd_;
        }

        int32_t StopBuffering()
        {
            index_ = 0.f;
            intIndex_ = 0;
            loopLength_ = bufferSamples_;
            intLoopLength_ = loopLength_;
            loopEnd_ = loopLength_ - 1.f;
            intLoopEnd_ = loopEnd_;
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
        inline float GetLoopEnd() { return loopEnd_; }
        inline float GetRate() { return rate_; }
        inline float GetPosition() { return index_; }
        inline int32_t GetIntPosition() { return intIndex_; }
        bool IsGoingForward() { return Direction::FORWARD == direction_; }
        bool IsRunning() { return RunStatus::RUNNING == runStatus_; }
        RunStatus GetRunStatus() { return runStatus_; }
    };
}