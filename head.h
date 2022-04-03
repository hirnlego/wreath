/**
 * Inspired by Monome softcut's subhead class:
 * https://github.com/monome/softcut-lib/blob/main/softcut-lib/src/SubHead.cpp
 */

#pragma once

#include "fader.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace wreath
{
    constexpr float kMinLoopLengthSamples{46.f}; // ~C1 @ 48KHz
    constexpr float kMinSamplesForTone{91.f};    // ~C2 @ 48KHz
    constexpr float kMinSamplesForFlanger{1722.f};

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

        void Init(float *buffer, float *buffer2, int32_t maxBufferSamples)
        {
            buffer_ = buffer;
            freezeBuffer_ = buffer2;
            maxBufferSamples_ = maxBufferSamples;
            rate_ = 1.f;
            looping_ = false;
            runStatus_ = RunStatus::STOPPED;
            movement_ = Movement::NORMAL;
            direction_ = Direction::FORWARD;
            samplesToFade_ = std::min(kSamplesToFade, loopLength_ / 2.f);
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
            return loopStart_;
        }

        float SetLoopLength(float length)
        {
            loopLength_ = length;
            intLoopLength_ = loopLength_;
            CalculateLoopEnd();
            samplesToFade_ = std::min(kSamplesToFade, loopLength_ / 2.f);

            return loopLength_;
        }

        inline void SetFreeze(float amount)
        {
            freezeAmount_ = amount;
            bool frozen = amount > 0;
            if (type_ == Type::READ)
            {
                frozen_ = frozen;
            }
            else
            {
                if (frozen_ && !frozen && !mustUnfreeze_)
                {
                    // Fade in recording in the freeze buffer.
                    mustUnfreeze_ = true;
                    freezeFadeIndex_ = 0;
                }
                else if (!frozen_ && frozen && !mustFreeze_)
                {
                    // Fade out recording in the freeze buffer.
                    mustFreeze_ = true;
                    freezeFadeIndex_ = 0;
                }
            }
        }

        inline void SetDegradation(float amount) { degradationAmount_ = amount; }

        inline void SetRate(float rate)
        {
            rate_ = std::abs(rate);
        }
        inline void SetMovement(Movement movement)
        {
            movement_ = movement;
        }

        inline void SetDirection(Direction direction)
        {
            direction_ = direction;
        }

        inline void SetIndex(float index)
        {
            index_ = index;
            intIndex_ = std::floor(index_);
        }

        inline void ResetPosition()
        {
            SetIndex(FORWARD == direction_ ? loopStart_ : loopEnd_);
        }

        bool UpdatePosition()
        {
            if (RunStatus::STOPPED == runStatus_)
            {
                //return false;
            }

            float index = index_ + (rate_ * direction_);
            SetIndex(index);
            Action action = HandleLoopAction();

            switch (action)
            {
            case STOP:
                if (READ == type_)
                {
                    Stop();
                }
                return true;

            case INVERT:
                if (READ == type_)
                {
                    ToggleDirection();
                }
                return true;

            case LOOP:
                return true;

            default:
                break;
            }

            return false;
        }

        float GetSamplesToFade()
        {
            return samplesToFade_;
        }

        void SetSamplesToFade(float samples)
        {
            samplesToFade_ = loopLength_ ? std::min(samples, loopLength_ / 2.f) : samples;
        }

        float ReadFrozen()
        {
            return frozen_ ? ReadAt(freezeBuffer_, index_) : 0;
        }

        float Read(float valueToFade)
        {
            if (RunStatus::STOPPED == runStatus_)
            {
                return 0.f;
            }

            float value = ReadAt(buffer_, index_);
            currentValue_ = value;

            valueToFade = 0.f;

            // Gradually start reading, fading from the input signal to the
            // buffered value.
            if (RunStatus::STARTING == runStatus_)
            {
                value = Fader::CrossFade(valueToFade, value, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::RUNNING;
                }
                fadeIndex_ += rate_;
            }
            // Gradually stop reading, fading from the buffered value to the
            // input signal.
            else if (RunStatus::STOPPING == runStatus_)
            {
                value = Fader::CrossFade(value, valueToFade, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::STOPPED;
                }
                fadeIndex_ += rate_;
            }

            return value;
        }

        float ReadBufferAt(float index, bool wrap = false)
        {
            return ReadAt(buffer_, index, wrap);
        }

        float ReadFrozenAt(float index, bool wrap = false)
        {
            return ReadAt(freezeBuffer_, index, wrap);
        }

        bool toggleOnset{true};
        int32_t previousE_{};
        bool BresenhamEuclidean(float pulses, float onsetAmount)
        {
            float ratio = bufferSamples_ / pulses;
            float onsets = onsetAmount * pulses;
            if (onsets == pulses)
            {
                toggleOnset = false;
            }
            else if (onsets == 0)
            {
                toggleOnset = true;
            }
            else
            {
                float slope = onsets / pulses;
                int32_t current = (index_ / ratio) * slope;
                if (current != previousE_)
                {
                    toggleOnset = !toggleOnset;
                }
                previousE_ = current;
            }
            return toggleOnset;
        }

        void HandleFreeze(float input)
        {
            float frozenValue = freezeBuffer_[intIndex_];
            if (mustFreeze_)
            {
                input = Fader::EqualCrossFade(input, frozenValue, freezeFadeIndex_ * (1.f / samplesToFade_));
                if (freezeFadeIndex_ >= samplesToFade_)
                {
                    mustFreeze_ = false;
                    frozen_ = true;
                }
                freezeFadeIndex_ += rate_;
            }
            else if (mustUnfreeze_)
            {
                input = Fader::EqualCrossFade(frozenValue, input, freezeFadeIndex_ * (1.f / samplesToFade_));
                if (freezeFadeIndex_ >= samplesToFade_)
                {
                    mustUnfreeze_ = false;
                    frozen_ = false;
                }
                freezeFadeIndex_ += rate_;
            }
            if (!frozen_ || mustUnfreeze_)
            {
                freezeBuffer_[intIndex_] = input;
            }
        }

        void Write(float input)
        {
            HandleFreeze(input);

            currentValue_ = buffer_[intIndex_];

            // Gradually start writing, fading from the buffered value to the input
            // signal.
            if (RunStatus::STARTING == runStatus_)
            {
                input = Fader::CrossFade(currentValue_, input, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::RUNNING;
                }
                fadeIndex_ += rate_;
            }
            // Gradually stop writing, fading from the input signal to the buffered
            // value.
            else if (RunStatus::STOPPING == runStatus_)
            {
                input = Fader::CrossFade(input, currentValue_, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::STOPPED;
                }
                fadeIndex_ += rate_;
            }

            buffer_[intIndex_] = input;
        }

        void ClearBuffer()
        {
            memset(buffer_, 0.f, maxBufferSamples_);
            memset(freezeBuffer_, 0.f, maxBufferSamples_);
        }

        bool Buffer(float value)
        {
            buffer_[intIndex_] = value;
            freezeBuffer_[intIndex_] = value;
            bufferSamples_ = intIndex_ + 1;

            // End of available buffer?
            if (intIndex_ >= maxBufferSamples_ - 1)
            {
                return true;
            }

            intIndex_++;

            return false;
        }

        void InitBuffer(int32_t bufferSamples)
        {
            bufferSamples_ = bufferSamples;
            loopLength_ = bufferSamples_;
            intLoopLength_ = loopLength_;
            loopEnd_ = loopLength_ - 1.f;
            intLoopEnd_ = loopEnd_;
            samplesToFade_ = std::min(kSamplesToFade, loopLength_ / 2.f);
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
            samplesToFade_ = std::min(kSamplesToFade, loopLength_ / 2.f);

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
            RunStatus status = loopLength_ > samplesToFade_ ? RunStatus::STARTING : RunStatus::RUNNING;
            SetRunStatus(status);

            return status;
        }

        RunStatus Stop()
        {
            // Stop right away if the loop length is smaller than the minimum
            // number of samples needed for a tone.
            RunStatus status = loopLength_ > samplesToFade_ ? RunStatus::STOPPING : RunStatus::STOPPED;
            SetRunStatus(status);

            return status;
        }

        void Activate()
        {
            active_ = true;
            ResetPosition();
            Start();
        }

        void Deactivate()
        {
            active_ = false;
            Stop();
        }

        void SetActive(bool active)
        {
            active_ = active;
        }

        void SetLooping(bool looping)
        {
            looping_ = looping;
        }

        void SetLoopSync(bool active)
        {
            loopSync_ = active;
        }

        inline int32_t GetBufferSamples() { return bufferSamples_; }
        inline float GetLoopEnd() { return loopEnd_; }
        inline float GetLoopLength() { return loopLength_; }
        inline float GetRate() { return rate_; }
        inline float GetPosition() { return index_; }
        inline int32_t GetIntPosition() { return intIndex_; }
        bool IsGoingForward() { return Direction::FORWARD == direction_; }
        bool IsRunning() { return RunStatus::RUNNING == runStatus_; }
        RunStatus GetRunStatus() { return runStatus_; }
        float GetCurrentValue() { return currentValue_; }

    private:
        const Type type_;
        float *buffer_;
        float *freezeBuffer_;

        int32_t maxBufferSamples_{}; // The whole buffer length in samples
        int32_t bufferSamples_{};    // The written buffer length in samples

        int32_t intIndex_{};
        float index_{};
        float rate_{};
        float fadeIndex_{};
        bool loopSync_{};

        float loopStart_{};
        int32_t intLoopStart_{};
        float loopEnd_{};
        int32_t intLoopEnd_{};
        float loopLength_{};
        int32_t intLoopLength_{};

        bool looping_{};
        bool active_{};
        RunStatus runStatus_{};

        Movement movement_{};
        Direction direction_{};

        float currentValue_{};
        float freezeAmount_{};
        float degradationAmount_{};
        bool frozen_{};

        bool mustFreeze_{};
        bool mustUnfreeze_{};
        float freezeFadeIndex_{};
        bool mustFadeInFrozen_{};
        float freezeLoopFadeIndex_{};

        float samplesToFade_{kSamplesToFade};

        Action HandleLoopAction()
        {
            if (!loopSync_)
            {
                // Handle normal loop boundaries.
                if (intLoopEnd_ > intLoopStart_)
                {
                    if (intIndex_ > intLoopEnd_)
                    {
                        if (intIndex_ >= bufferSamples_)
                        {
                            SetIndex(index_ - bufferSamples_);
                        }

                        return looping_ ? Action::LOOP : Action::STOP;
                    }
                    else if (intIndex_ < intLoopStart_)
                    {
                        if (intIndex_ < 0)
                        {
                            SetIndex(bufferSamples_ + index_);
                        }

                        return looping_ ? Action::LOOP : Action::STOP;
                    }
                }
                // Handle inverted loop boundaries (end point comes before start point).
                else
                {
                    if (intIndex_ > intLoopEnd_ && intIndex_ < intLoopStart_)
                    {
                        return looping_ ? Action::LOOP : Action::STOP;
                    }
                    else if (intIndex_ >= bufferSamples_)
                    {
                        SetIndex(index_ - bufferSamples_);
                    }
                    else if (intIndex_ < 0)
                    {
                        SetIndex(bufferSamples_ + index_);
                    }
                }

                return Action::NO_ACTION;
            }

            // Handle normal loop boundaries.
            if (intLoopEnd_ > intLoopStart_)
            {
                // Forward direction.
                if (Direction::FORWARD == direction_)
                {
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
                    else if (RunStatus::RUNNING == runStatus_ && !looping_ && intIndex_ > intLoopEnd_ - samplesToFade_)
                    {
                        return Action::STOP;
                    }
                }
                // Backwards direction.
                else
                {
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
                    else if (!looping_ && intIndex_ < intLoopStart_ + samplesToFade_)
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

        float ReadAt(float *buffer, float index, bool wrap = false)
        {
            int32_t intPos = wrap ? WrapIndex(index) : index;
            float value = buffer[intPos];
            float frac = index - intPos;

            // Interpolate value only it the index has a fractional part.
            if (frac > std::numeric_limits<float>::epsilon())
            {
                value = value + (buffer[WrapIndex(intPos + direction_)] - value) * frac;
            }

            return value;
        }
    };
}