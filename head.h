/**
 * Inspired by Monome softcut's subhead class:
 * https://github.com/monome/softcut-lib/blob/main/softcut-lib/src/SubHead.cpp
 */

#pragma once

#include <stdint.h>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace wreath
{
    constexpr int32_t kMinLoopLengthSamples{48};
    constexpr int32_t kMinLoopLengthForFade{4800};
    constexpr int32_t kSamplesToFade{1200};

    enum Type
    {
        READ,
        WRITE,
    };

    enum Action
    {
        NONE,
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

        int32_t loopStart_{};
        int32_t loopEnd_{};
        int32_t loopLength_{};

        int fadeIndex_{};
        int fadeSamples_{};
        bool fading_{};

        bool run_{};
        bool loop_{};

        Movement movement_{};
        Direction direction_{};

        static inline float Hermite(float x, float y0, float y1, float y2, float y3)
        {
            return (((0.5f * (y3 - y0) + 1.5f * (y1 - y2)) * x + (y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3)) * x + 0.5f * (y2 - y0)) * x + y1;
        }

        Action HandleLoopAction()
        {
            float index{};

            // Handle normal loop boundaries.
            if (loopEnd_ > loopStart_)
            {
                // Forward direction.
                if (FORWARD == direction_ && intIndex_ > loopEnd_)
                {
                    if (PENDULUM == movement_ && loop_)
                    {
                        SetIndex(loopEnd_ - (index_ - loopEnd_));

                        return INVERT;
                    }
                    else
                    {
                        SetIndex((loopStart_ + (index_ - loopEnd_)) - 1);

                        return loop_ ? LOOP : STOP;
                    }
                }
                // Backwards direction.
                else if (BACKWARDS == direction_ && intIndex_ < loopStart_)
                {
                    if (PENDULUM == movement_ && loop_)
                    {
                        SetIndex(loopStart_ + (loopStart_ - index_));

                        return INVERT;
                    }
                    else
                    {
                        SetIndex((loopEnd_ - std::abs(loopStart_ - index_)) + 1);

                        return loop_ ? LOOP : STOP;
                    }
                }
            }
            // Handle inverted loop boundaries (end point comes before start point).
            else
            {
                float frame = bufferSamples_ - 1;
                if (FORWARD == direction_)
                {
                    if (intIndex_ > frame)
                    {
                        // Wrap-around.
                        SetIndex((index_ - frame) - 1);

                        return loop_ ? LOOP : STOP;
                    }
                    else if (intIndex_ > loopEnd_ && intIndex_ < loopStart_)
                    {
                        if (PENDULUM == movement_ && loop_)
                        {
                            // Max to avoid overflow.
                            SetIndex(std::max(loopEnd_ - (index_ - loopEnd_), 0.f));

                            return INVERT;
                        }
                        else
                        {
                            // Min to avoid overflow.
                            SetIndex(std::min(loopStart_ + (index_ - loopEnd_) - 1, frame));

                            return loop_ ? LOOP : STOP;
                        }
                    }
                }
                else
                {
                    if (intIndex_ < 0)
                    {
                        // Wrap-around.
                        SetIndex((frame - std::abs(index_)) + 1);

                        return loop_ ? LOOP : STOP;
                    }
                    else if (intIndex_ > loopEnd_ && intIndex_ < loopStart_)
                    {
                        if (PENDULUM == movement_ && loop_)
                        {
                            // Min to avoid overflow.
                            SetIndex(std::min(loopStart_ + (loopStart_ - index_), frame));

                            return INVERT;
                        }
                        else
                        {
                            // Max to avoid overflow.
                            SetIndex(std::max(loopEnd_ - (loopStart_ - index_) + 1, 0.f));

                            return loop_ ? LOOP : STOP;
                        }
                    }
                }
            }

            return NONE;
        }

        int32_t WrapIndex(int32_t index)
        {
            // Handle normal loop boundaries.
            if (loopEnd_ > loopStart_)
            {
                // Forward direction.
                if (index > loopEnd_)
                {
                    if (PENDULUM == movement_)
                    {
                        index = loopEnd_ - (index - loopEnd_);
                    }
                    else
                    {
                        index = (FORWARD == direction_) ? (loopStart_ + (index - loopEnd_)) - 1: 0;
                    }
                }
                // Backwards direction.
                else if (index < loopStart_)
                {
                    if (PENDULUM == movement_)
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
                        index = (PENDULUM == movement_) ? std::max(loopEnd_ - (index - loopEnd_), static_cast<int32_t>(0)) : std::min(loopStart_ + (index - loopEnd_) - 1, frame);
                    }
                    else
                    {
                        // Max/min to avoid overflow.
                        index = (PENDULUM == movement_) ? std::min(loopStart_ + (loopStart_ - index), frame) : std::max(loopEnd_ - (loopStart_ - index) + 1, static_cast<int32_t>(0));
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

    public:
        Head(Type type): type_{type} {}
        ~Head() {}

        void Reset()
        {
            intIndex_ = 0;
            index_ = 0.f;
            rate_ = 1.f;
            loopStart_ = 0;
            loopEnd_ = 0;
            loopLength_ = 0;
            run_ = true;
            loop_ = true;
            movement_ = NORMAL;
            direction_ = FORWARD;
        }

        void Init(float *buffer, int32_t maxBufferSamples)
        {
            buffer_ = buffer;
            maxBufferSamples_ = maxBufferSamples;
            Reset();
        }

        int32_t SetLoopStart(int32_t start)
        {
            loopStart_ = std::min(std::max(start, static_cast<int32_t>(0)), bufferSamples_ - 1);
            CalculateLoopEnd();

            return loopStart_;
        }

        int32_t SetLoopLength(int32_t length)
        {
            loopLength_ = std::min(std::max(length, static_cast<int32_t>(kMinLoopLengthSamples)), bufferSamples_);
            CalculateLoopEnd();

            return loopLength_;
        }

        int32_t SamplesToFade()
        {
            return std::min(kSamplesToFade, loopLength_ / 2);
        }

        inline void SetRate(float rate) { rate_ = rate; }
        inline void SetMovement(Movement movement) { movement_ = movement; }
        inline void SetDirection(Direction direction) { direction_ = direction; }
        inline void SetIndex(float index)
        {
            index_ = index;
            intIndex_ = static_cast<int32_t>(std::floor(index));
        }
        inline void ResetPosition()
        {
            SetIndex(FORWARD == direction_  ? loopStart_ : loopEnd_);
            // Fade when resetting position.
            //Fade();
        }
        float UpdatePosition()
        {
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
                    // Fade when changing direction.
                    //Fade();
                    break;

                case LOOP:
                    // Fade when looping.
                    //Fade();
                    break;

                default:
                    break;
                }
            }

            return index_;
        }

        float Read()
        {
            float value = ReadAt(index_);
            /*
            if (rate_ > 1.f)
            {
                int32_t phase0 = intIndex_ - 1 * direction_;
                int32_t phase2 = intIndex_ + 1 * direction_;
                int32_t phase3 = intIndex_ + 2 * direction_;

                float y0 = buffer_[WrapIndex(phase0)];
                float y2 = buffer_[WrapIndex(phase2)];
                float y3 = buffer_[WrapIndex(phase3)];

                float x = index_ - intIndex_;

                value = Hermite(x, y0, value, y2, y3);
            }
            */

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
            buffer_[WrapIndex(intIndex_)] = value;
        }

        void ClearBuffer()
        {
            memset(buffer_, 0.f, maxBufferSamples_);
        }

        bool Buffer(float value)
        {
            buffer_[intIndex_] = value;

            // Handle end of buffer.
            if (bufferSamples_ > maxBufferSamples_ - 1)
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
            loopEnd_ = loopLength_ - 1 ;
        }

        int32_t StopBuffering()
        {
            intIndex_ = 0;
            loopLength_ = bufferSamples_;
            loopEnd_ = loopLength_ - 1 ;
            ResetPosition();

            return bufferSamples_;
        }

        inline Direction ToggleDirection()
        {
            direction_ = static_cast<Direction>(direction_ * -1);

            return direction_;
        }
        inline void Run()
        {
            run_ = true;
        }
        inline void Stop()
        {
            run_ = false;
        }
        inline bool ToggleRun()
        {
            run_ = !run_;

            return run_;
        }

        inline int32_t GetBufferSamples() { return bufferSamples_; }
        inline int32_t GetLoopEnd() { return loopEnd_; }
        inline float GetRate() { return rate_; }
        inline float GetPosition() { return index_; }
        inline int32_t GetIntPosition() { return intIndex_; }
        bool IsGoingForward() { return FORWARD == direction_; }
    };
}