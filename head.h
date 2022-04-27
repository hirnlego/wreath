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

    class Head
    {
    public:
        Head(Type type) : type_{type} {}
        ~Head() {}

        enum class Action
        {
            NO_ACTION,
            LOOP,
            INVERT,
            STOP,
        };

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

        void SetLoopStartAndLength(float start, float length)
        {
            loopStart_ = start;
            intLoopStart_ = loopStart_;
            loopLength_ = length;
            intLoopLength_ = loopLength_;
            CalculateLoopEnd();
            samplesToFade_ = std::min(kSamplesToFade, loopLength_ / 2.f);
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

        inline void SetOffset(float offset)
        {
            offset_ = offset;
        }

        inline void ResetPosition()
        {
            SetIndex(FORWARD == direction_ ? loopStart_ + offset_ : loopEnd_ - offset_);
        }

        Action UpdatePosition()
        {
            if (!active_)
            {
                return Action::NO_ACTION;
            }

            float index = index_ + (rate_ * direction_);
            SetIndex(index);
            Action action = HandleLoopAction();

            if (intIndex_ >= bufferSamples_)
            {
                SetIndex(index_ - bufferSamples_);
            }
            else if (intIndex_ < 0)
            {
                SetIndex(bufferSamples_ + index_);
            }

            switch (action)
            {
            case Action::INVERT:
                if (READ == type_)
                {
                    ToggleDirection();
                }
                break;

            default:
                break;
            }

            return action;
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

        float Read()
        {
            return ReadAt(buffer_, index_);
        }

        bool toggleOnset{true};
        int32_t previousE_{};
        /**
         * @brief Bresenham implementation of an Euclidean Rhythm Algorithm.
         */
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
        inline float GetOffset() { return offset_; }
        inline int32_t GetIntPosition() { return intIndex_; }
        bool IsGoingForward() { return Direction::FORWARD == direction_; }

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

        bool active_{};
        bool looping_{};

        Movement movement_{};
        Direction direction_{};

        float freezeAmount_{};
        float degradationAmount_{};
        bool frozen_{};

        bool mustFreeze_{};
        bool mustUnfreeze_{};
        float freezeFadeIndex_{};
        bool mustFadeInFrozen_{};
        float freezeLoopFadeIndex_{};

        float samplesToFade_{kSamplesToFade};

        float offset_{};

        Action HandleLoopAction()
        {
            // Handle normal loop boundaries.
            if (intLoopEnd_ > intLoopStart_)
            {
                if (looping_ && ((Direction::FORWARD == direction_ && index_ > loopEnd_) || (Direction::BACKWARDS == direction_ && index_ < loopStart_)))
                {
                    offset_ = rate_ != 1.f ? (Direction::FORWARD == direction_ ? index_ - loopEnd_ : loopStart_ - index_) : 0;

                    return Action::LOOP;
                }
                if (!looping_ && ((Direction::FORWARD == direction_ && index_ >= loopEnd_ - samplesToFade_) || (Direction::BACKWARDS == direction_ && index_ <= loopStart_ + samplesToFade_)))
                {
                    offset_ = 0;

                    return Action::STOP;
                }
            }
            // Handle inverted loop boundaries (end point comes before start point).
            else
            {
                if (looping_ && index_ > loopEnd_ && index_ < loopStart_)
                {
                    offset_ = rate_ != 1.f ? (Direction::FORWARD == direction_ ? index_ - loopEnd_ : loopStart_ - index_) : 0;

                    return Action::LOOP;
                }
                if (!looping_ && ((Direction::FORWARD == direction_ && index_ >= loopEnd_ - samplesToFade_ && index_ < loopStart_) || (Direction::BACKWARDS == direction_ && index_ <= loopStart_ + samplesToFade_ && index_ > loopEnd_)))
                {
                    offset_ = 0;

                    return Action::STOP;
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

        float ReadAt(float *buffer, float index)
        {
            int32_t intPos = index;
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